// HERO VR 0.0.77 cut4 FRAME TIME (diagnostic): a light once-a-second line - the VR loop's average and worst
// frame, and the microseconds spent in each path new since story8. Atomics only; no GPU readback.
#pragma once
#include <atomic>
#include <cstdint>
#include <windows.h>

namespace spidy::perf077 {
enum Path : unsigned { cards = 0, cameraSample = 1, poiDrain = 2, pathCount = 3 };
inline const char* pathName(unsigned p) {
    static const char* names[pathCount] = {"cards+waypoint", "camera sample", "poi dump"};
    return p < pathCount ? names[p] : "?";
}
inline std::atomic<uint64_t> pathUs[pathCount]{};
inline std::atomic<uint64_t> pathPeakUs[pathCount]{};
inline int64_t qpcNow() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}
inline double qpcUs(int64_t ticks) {
    static const double f = [] {
        LARGE_INTEGER q;
        QueryPerformanceFrequency(&q);
        return static_cast<double>(q.QuadPart);
    }();
    return static_cast<double>(ticks) * 1e6 / f;
}
inline void add(unsigned p, uint64_t us) {
    if (p >= pathCount)
        return;
    pathUs[p].fetch_add(us, std::memory_order_relaxed);
    uint64_t peak = pathPeakUs[p].load(std::memory_order_relaxed);
    while (us > peak && !pathPeakUs[p].compare_exchange_weak(peak, us, std::memory_order_relaxed)) {
    }
}
struct Scope {
    unsigned path;
    int64_t start = qpcNow();
    explicit Scope(unsigned p) : path(p) {}
    ~Scope() { add(path, static_cast<uint64_t>(qpcUs(qpcNow() - start))); }
};
// The VR loop's frames: call once a loop; true (and the line filled) once a second.
struct Frames {
    int64_t last{}, windowStart{};
    double sumMs{}, worstMs{};
    uint32_t count{};
    // CUT7: `windowSeconds` 1 (live.txt diag_logs full) or 10 (quiet, the default).
    bool tick(char* line, size_t size, unsigned windowSeconds = 1) {
        const int64_t now = qpcNow();
        if (!last) {
            last = windowStart = now;
            return false;
        }
        const double ms = qpcUs(now - last) / 1000.0;
        last = now;
        sumMs += ms;
        worstMs = ms > worstMs ? ms : worstMs;
        ++count;
        if (qpcUs(now - windowStart) < 1e6 * (windowSeconds ? windowSeconds : 1))
            return false;
        int n = sprintf_s(line, size, "HERO VR 0.0.77 FRAME (VR loop): %u frames, avg %.1f ms, worst %.1f ms;", count,
                          count ? sumMs / count : 0.0, worstMs);
        for (unsigned p = 0; p < pathCount && n > 0 && static_cast<size_t>(n) < size; ++p) {
            const int m = sprintf_s(line + n, size - n, " %s %.2f ms/s peak %.2f ms", pathName(p),
                                    pathUs[p].exchange(0) / 1000.0, pathPeakUs[p].exchange(0) / 1000.0);
            if (m <= 0)
                break;
            n += m;
        }
        windowStart = now;
        sumMs = worstMs = 0;
        count = 0;
        return true;
    }
};
} // namespace spidy::perf077
