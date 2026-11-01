// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// 0.0.76 SUIT PROBE (include/spidy/suit_probe.hpp). READ-ONLY: every read of the game goes through peek()
// (SEH); nothing here writes game memory, calls a game function, changes page protection or makes a hook.
#include <windows.h>

#include <cstring>
#include <exception>

#include "spidy/suit_probe.hpp"

namespace spidy::suit_probe {
namespace {
bool peek(uint64_t p, void* out, size_t n) {
    __try {
        if (p < lowest || p >= highest)
            return false;
        std::memcpy(out, reinterpret_cast<const void*>(p), n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
uint64_t peek64(uint64_t p) {
    uint64_t v{};
    return peek(p, &v, 8) ? v : 0;
}
Watch watch;          // the VR loop's thread only
ULONGLONG lastScan{}; // GetTickCount64
bool wasOn{};
} // namespace

void frame(uint64_t record, bool on, void (*log)(const char*)) noexcept {
    try {
        const ULONGLONG now = GetTickCount64();
        if (on == wasOn && (!on || now - lastScan < 1000))
            return;
        wasOn = on;
        lastScan = now;
        std::vector<Hit> hits;
        if (on && record) {
            const uint64_t roots[2] = {record, peek64(record)};
            auto reader = [](uint64_t p, void* out, size_t n) { return peek(p, out, n); };
            hits = scan(roots, reader);
        }
        for (const auto& line : watch.update(on, record, hits))
            if (log)
                log(line.c_str());
    } catch (...) {
    }
}

} // namespace spidy::suit_probe
