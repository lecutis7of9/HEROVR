// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// Update 38.5 BALL LOOK WRIST (include/spidy/wrist_muzzle.hpp says what is known and what is not).
// The hooks only change the web shooter's muzzle position (the 4x4's row 3) within
// `window` of this port's own ball press; every other call returns the game's own result.
#include "spidy/wrist_muzzle.hpp"
#include <MinHook.h>
#include <atomic>
#include <cstring>
#include <windows.h>
using namespace spidy;
namespace wm = spidy::wrist_muzzle;

extern "C" {
__declspec(dllexport) wm::Status SpidyWristMuzzleData{};
}
namespace {
using Muzzle = float* (*)(void*, float*, uint32_t);
using ParentMuzzle = float* (*)(void*, float*, const float*, uint32_t);
Muzzle originalMuzzle{};
ParentMuzzle originalParent{};
void* muzzleHook{};
void* parentHook{};
uintptr_t base{};
std::atomic<bool> enabled{true}, running{};
SRWLOCK lock = SRWLOCK_INIT; // wrists, shot, the counters
wm::Wrist wrists[2]{};
wm::Shot shot{};
LARGE_INTEGER frequency{};
float seconds() {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return frequency.QuadPart ? static_cast<float>(static_cast<double>(now.QuadPart) / static_cast<double>(frequency.QuadPart)) : 0.f;
}
bool read(uintptr_t p, void* out, size_t n) {
    __try {
        if (p < 0x10000)
            return false;
        std::memcpy(out, reinterpret_cast<void*>(p), n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
template <class F> void update(F change) {
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&SpidyWristMuzzleData.sequence));
    change(SpidyWristMuzzleData);
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&SpidyWristMuzzleData.sequence));
}
bool webShooter(void* weapon) {
    uintptr_t vtable{};
    return weapon && read(reinterpret_cast<uintptr_t>(weapon), &vtable, sizeof(vtable)) &&
           vtable == base + wm::webShooterVtable;
}
// The web shooter's muzzle just made by the game: moved to the wrist when it is this ball's.
void adjust(void* weapon, float* out, uint32_t index) {
    if (!out)
        return;
    AcquireSRWLockExclusive(&lock);
    update([&](wm::Status& s) {
        ++s.calls;
        if (!webShooter(weapon))
            return;
        ++s.webCalls;
        ++s.indexSeen[index < 3 ? index : 3];
        if (!enabled.load(std::memory_order_relaxed))
            return;
        const float now = seconds();
        const Vec3 game{out[12], out[13], out[14]};
        Vec3 to{};
        switch (wm::decide(shot, wrists, now, game, to)) {
        case wm::moved:
            ++s.moved;
            s.lastShift = length(to - game);
            s.lastAge = now - shot.at;
            out[12] = to.x;
            out[13] = to.y;
            out[14] = to.z;
            break;
        case wm::noShot: ++s.noShot; break;
        case wm::stale: ++s.stale; break;
        case wm::tooFar: ++s.tooFar; break;
        case wm::notFinite: ++s.notFinite; break;
        default: break;
        }
    });
    ReleaseSRWLockExclusive(&lock);
}
float* muzzle(void* weapon, float* out, uint32_t index) {
    float* result = originalMuzzle(weapon, out, index);
    adjust(weapon, out, index);
    return result;
}
float* parentMuzzle(void* weapon, float* out, const float* parent, uint32_t index) {
    float* result = originalParent(weapon, out, parent, index);
    adjust(weapon, out, index);
    return result;
}
bool bytesAre(uintptr_t rva, const unsigned char* expected, size_t n) {
    unsigned char b[32]{};
    return n <= sizeof(b) && read(base + rva, b, n) && !std::memcmp(b, expected, n);
}
// The vtable's +0x168 / +0x160 slots must still point at the functions hooked.
bool slotIs(uintptr_t slot, uintptr_t rva) {
    uintptr_t f{};
    return read(base + wm::webShooterVtable + slot, &f, sizeof(f)) && f == base + rva;
}
} // namespace

uint32_t wm::start(uintptr_t gameBase) {
    if (running)
        return 0;
    base = gameBase;
    QueryPerformanceFrequency(&frequency);
    uint32_t error = 0;
    if (!base || !bytesAre(muzzleRva, muzzleBytes, sizeof(muzzleBytes)) || !slotIs(0x168, muzzleRva))
        error = 11001;
    if (!error) {
        const auto s = MH_Initialize();
        if (s != MH_OK && s != MH_ERROR_ALREADY_INITIALIZED)
            error = 11100u + static_cast<uint32_t>(s);
    }
    uint32_t hooked = 0;
    if (!error) {
        muzzleHook = reinterpret_cast<void*>(base + muzzleRva);
        if (const auto c = MH_CreateHook(muzzleHook, reinterpret_cast<void*>(muzzle),
                                         reinterpret_cast<void**>(&originalMuzzle));
            c != MH_OK)
            error = 11200u + static_cast<uint32_t>(c);
        else if (const auto e = MH_EnableHook(muzzleHook); e != MH_OK)
            error = 11300u + static_cast<uint32_t>(e);
        else
            hooked |= 1;
    }
    // The parent-matrix variant: only alongside the first, and only when it checks out.
    if (hooked && bytesAre(parentMuzzleRva, parentMuzzleBytes, sizeof(parentMuzzleBytes)) &&
        slotIs(0x160, parentMuzzleRva)) {
        parentHook = reinterpret_cast<void*>(base + parentMuzzleRva);
        if (MH_CreateHook(parentHook, reinterpret_cast<void*>(parentMuzzle),
                          reinterpret_cast<void**>(&originalParent)) == MH_OK &&
            MH_EnableHook(parentHook) == MH_OK)
            hooked |= 2;
        else
            parentHook = nullptr;
    }
    running = hooked != 0;
    AcquireSRWLockExclusive(&lock);
    update([&](Status& s) {
        s.hooked = hooked;
        s.startError = error;
        s.enabled = enabled ? 1u : 0u;
    });
    ReleaseSRWLockExclusive(&lock);
    return error;
}
void wm::stop() {
    if (!running)
        return;
    for (void* h : {muzzleHook, parentHook})
        if (h)
            MH_DisableHook(h);
    running = false;
}
void wm::publish(unsigned hand, Vec3 world, bool valid) {
    if (hand > 1)
        return;
    const float now = seconds();
    AcquireSRWLockExclusive(&lock);
    wrists[hand] = {world, now, valid && finite(world)};
    ReleaseSRWLockExclusive(&lock);
}
void wm::fire(unsigned hand) {
    if (hand > 1)
        return;
    const float now = seconds();
    AcquireSRWLockExclusive(&lock);
    shot = {static_cast<int>(hand), now};
    update([&](Status& s) { ++s.shots[hand]; });
    ReleaseSRWLockExclusive(&lock);
}
void wm::setEnabled(bool on) {
    enabled = on;
    AcquireSRWLockExclusive(&lock);
    update([&](Status& s) { s.enabled = on ? 1u : 0u; });
    ReleaseSRWLockExclusive(&lock);
}
wm::Status wm::status() {
    AcquireSRWLockShared(&lock);
    const Status s = SpidyWristMuzzleData;
    ReleaseSRWLockShared(&lock);
    return s;
}
