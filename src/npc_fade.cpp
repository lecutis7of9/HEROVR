// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// Update 37.2: NPC NEAR FADE (npc_fade.hpp). A low-priority thread, four times a second:
// puts the mode's two bytes in place (only where the exe's bytes are the expected ones),
// puts the game's back when the mode or VR ends, and logs near-person episodes.
#include "spidy/npc_fade.hpp"
#include "spidy/world_trace.hpp"
#include <atomic>
#include <cstring>
#include <thread>
#include <windows.h>

using namespace spidy;
using namespace spidy::npc_fade;

extern "C" __declspec(dllexport) npc_fade::Data SpidyNpcFadeData;
npc_fade::Data SpidyNpcFadeData;

namespace {
std::atomic<uint32_t> wanted{defaultMode}, inForce{game}, why{fine};
std::atomic<bool> running{};
std::thread worker;
uintptr_t base{};
bool known{};       // the exe's bytes are the expected ones
bool retPatched{};  // our c3 is over the hit-hide's first byte
bool switchSet{};   // we set the switch

bool read(uintptr_t p, void* out, size_t n) {
    __try {
        std::memcpy(out, reinterpret_cast<const void*>(p), n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool same(uint32_t rva, const unsigned char* bytes, size_t n) {
    unsigned char now[16]{};
    return n <= sizeof now && read(base + rva, now, n) && !std::memcmp(now, bytes, n);
}
uint8_t switchByte() {
    uint8_t v{};
    read(base + switchRva, &v, 1);
    return v;
}
bool writeSwitch(uint8_t v) {
    __try {
        std::atomic_ref<uint8_t>(*reinterpret_cast<uint8_t*>(base + switchRva)).store(v);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// One byte of code: c3 (return at once) or the game's 48 back. Only from the other one.
bool writeRet(bool patched) {
    if (retPatched == patched)
        return true;
    auto* at = reinterpret_cast<uint8_t*>(base + hitHideRva);
    uint8_t now{};
    if (!read(base + hitHideRva, &now, 1) || now != (patched ? hitHideBytes[0] : retByte))
        return false;
    DWORD protection{};
    if (!VirtualProtect(at, 1, PAGE_EXECUTE_READWRITE, &protection))
        return false;
    std::atomic_ref<uint8_t>(*at).store(patched ? retByte : hitHideBytes[0]);
    DWORD unused{};
    VirtualProtect(at, 1, protection, &unused);
    FlushInstructionCache(GetCurrentProcess(), at, 1);
    retPatched = patched;
    return true;
}
void publish(const Data& work) {
    auto& d = SpidyNpcFadeData;
    const uint64_t s = d.sequence;
    reinterpret_cast<std::atomic<uint64_t>&>(d.sequence).store(s + 1, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_release);
    std::memcpy(static_cast<void*>(&d), &work, offsetof(Data, sequence));
    std::atomic_thread_fence(std::memory_order_release);
    reinterpret_cast<std::atomic<uint64_t>&>(d.sequence).store(s + 2, std::memory_order_release);
}
// Puts `mode` in force; any failure: everything back to the game's, GAME, and why.
void apply(uint32_t mode, Data& work) {
    if (!known) {
        inForce = game;
        return;
    }
    const bool before = retPatched;
    bool ok = writeRet(wantsRet(mode));
    if (retPatched != before)
        ++work.retWrites;
    if (ok && wantsSwitch(mode)) {
        if (!switchByte()) {
            if (switchSet)
                ++work.switchCleared; // the game's action ended and cleared it: set again
            ok = writeSwitch(1);
            if (ok)
                ++work.switchWrites;
        }
        switchSet = ok;
    } else if (switchSet) {
        writeSwitch(0); // only ever clear what we set
        switchSet = false;
    }
    if (!ok) {
        writeRet(false);
        if (switchSet)
            writeSwitch(0);
        switchSet = false;
        why = writeFailed;
        wanted = game;
        inForce = game;
        return;
    }
    inForce = mode;
}
void run() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    Data work;
    work.bytes = sizeof(Data);
    known = same(gateRva, gateBytes, sizeof gateBytes) && same(writerRva, writerBytes, sizeof writerBytes) &&
            same(hitHideRva, hitHideBytes, sizeof hitHideBytes) &&
            same(callerRvas[0], callerBytes[0], 5) && same(callerRvas[1], callerBytes[1], 5);
    if (!known) {
        why = noBytes;
        wanted = game;
    }
    Episodes episodes;
    while (running.load()) {
        const uint32_t m = wanted.load();
        if (m != inForce.load() || (wantsSwitch(m) && known && !switchByte()))
            apply(m, work);
        const float metres = world_trace::nearestPersonMetres();
        if (episodes.step(metres)) {
            ++work.nearEpisodes;
            ++(inForce.load() == game ? work.nearWhileGame : work.nearWhileFixed);
        }
        work.mode = inForce.load();
        work.why = why.load();
        work.ready = known ? 1 : 0;
        work.switchNow = known ? switchByte() : 0;
        work.retNow = retPatched ? 1 : 0;
        work.nearestMetres = metres;
        publish(work);
        Sleep(250);
    }
    apply(game, work); // VR ends: the game's bytes back
    work.mode = game;
    work.retNow = retPatched ? 1 : 0;
    publish(work);
}
} // namespace

namespace spidy::npc_fade {
void start(uintptr_t imageBase) {
    if (running.exchange(true))
        return;
    base = imageBase;
    worker = std::thread(run);
}
void stop() {
    if (!running.exchange(false))
        return;
    if (worker.joinable())
        worker.join();
}
uint32_t now() { return inForce.load() | why.load() << 8; }
void set(uint32_t mode) {
    if (why.load() == noBytes)
        return;
    why = fine;
    wanted = mode % modes;
}
} // namespace spidy::npc_fade

// Live setting npc_fade: 0 GAME, 1 OFF, 2 SWITCH, 3 the next one (the hologram's press).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyNpcFade(void* mode) {
    const auto v = reinterpret_cast<uintptr_t>(mode);
    if (v > nextMode)
        return 5193;
    if (why.load() == noBytes)
        return 5194;
    npc_fade::set(v == nextMode ? npc_fade::next(wanted.load()) : static_cast<uint32_t>(v));
    return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyNpcFadeNow(void*) { return npc_fade::now(); }
