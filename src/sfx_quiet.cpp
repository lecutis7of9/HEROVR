// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Version 0.0.5 SOUND EFFECTS: the stand-in runtime for the tests (no audio): the requests are
// counted and left for the test to take. The game's module links src/sfx_win.cpp instead.
#include "spidy/sfx.hpp"

namespace spidy::sfx {
namespace {
std::atomic<float> quietGain{1.f};
std::atomic<uint64_t> quietAsked{}, quietMuted{};
} // namespace
bool start() { return false; }
void play(Id id) {
    if (id >= sounds)
        return;
    quietAsked.fetch_add(1, std::memory_order_relaxed);
    if (quietGain.load(std::memory_order_relaxed) <= 0.f)
        quietMuted.fetch_add(1, std::memory_order_relaxed);
    else
        requests().play(id);
}
void setGain(float gain) { quietGain.store(gain >= 0.f && gain <= 1.f ? gain : (gain > 1.f ? 1.f : 0.f), std::memory_order_relaxed); }
Stats stats() {
    Stats s;
    s.asked = quietAsked.load();
    s.muted = quietMuted.load();
    s.gain = quietGain.load();
    return s;
}
} // namespace spidy::sfx
