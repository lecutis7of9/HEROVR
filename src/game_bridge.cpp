// Game-specific integration experiments. All functions are signature checked;
// controls require a short renewable lease, and camera writes target only the
// camera whose target record matches the independently discovered local player.
#include "spidy/game_bridge_protocol.hpp"
#include <MinHook.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <intrin.h>
#include <windows.h>

using namespace spidy::bridge;
extern "C" {
__declspec(dllexport) Data SpidyBridgeData;
}

namespace {
uintptr_t base{};
// The local player. Zero until a save is loaded; SpidyRetarget follows it
// to each new actor (a loaded save, a respawn, a character switch).
std::atomic<uintptr_t> record{}, hero{};
SRWLOCK lifecycle = SRWLOCK_INIT, controls = SRWLOCK_INIT, telemetry = SRWLOCK_INIT;
Control control;
uint64_t deadline{};
std::atomic<bool> enabled{false};
std::atomic<uint64_t> inputFrames{}, digitalQueries{}, analogQueries{}, inputServed{}, expired{};
std::atomic<uint32_t> keyStates[15]{};
std::atomic<uint32_t> queriedKeys[8]{};
std::atomic<uintptr_t> inputSelf{};
using Commit = void (*)(void*, void*);
// Camera2 mover update (vtable slot 21): the mover, its CameraTarget, the
// frame's seconds and two update flags.
using MoverUpdate = void (*)(void*, void*, float, bool, bool);
using ClearInput = void (*)(void*, uint64_t);
using Digital = uint32_t (*)(void*, uint32_t, uint32_t);
using Analog = float (*)(void*, uint32_t, uint32_t);
using SetPosition = void (*)(void*, const float*);
// The player's cameras (Camera2 vtables, image offsets). The follow camera's
// update ends in the commit (slot 24, 1e1d600); the combat camera's update
// (5c2630) places its camera itself with SetTranslation and never commits, so
// before it was hooked every fight looked like a cutscene to the VR gate.
constexpr uintptr_t followMover = followCamera, combatMover = combatCamera, combatUpdateRva = 0x9907c0; // Spider-Man 2, from SM2VR
Commit originalCommit{};
MoverUpdate originalCombat{};
ClearInput originalClear{};
Digital originalDigital{};
Analog originalAnalog{};
std::array<void*, 5> hooks{};
bool created{};

bool read(uintptr_t address, void* out, size_t bytes) {
    __try {
        if (address < 0x10000)
            return false;
        std::memcpy(out, reinterpret_cast<void*>(address), bytes);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
uintptr_t pointer(uintptr_t address) {
    uintptr_t out{};
    read(address, &out, sizeof(out));
    return out;
}
bool transform(uintptr_t address, float* out) {
    if (!read(address, out, 64))
        return false;
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(out[i]) || std::abs(out[i]) > 1e6f)
            return false;
    for (int row = 0; row < 3; ++row) {
        float norm{};
        for (int col = 0; col < 3; ++col)
            norm += out[row * 4 + col] * out[row * 4 + col];
        if (std::abs(norm - 1) > .05f)
            return false;
    }
    return true;
}
Control current() {
    AcquireSRWLockShared(&controls);
    Control out = control;
    if (!enabled.load() || GetTickCount64() >= deadline)
        out.modes = out.keys = 0;
    ReleaseSRWLockShared(&controls);
    return out;
}
bool isPlayer(uintptr_t h, uintptr_t r) {
    // SM2: the hero component's vtable, and its record at +10 (was +8), from SM2VR.
    return h && r && pointer(h) == base + 0x5424128 && pointer(h + 0x10) == r && pointer(r) != 0;
}
bool livePlayer() {
    return isPlayer(hero.load(), record.load());
}
// A camera has placed itself for this frame (mover + 0x10 resolves to its
// transform). Only the follow camera's commit takes the camera-offset
// experiment's write; the combat camera is observed.
void placed(uintptr_t mover, uintptr_t targetAddress, uintptr_t caller, bool writable) {
    const auto vt = pointer(mover);
    const auto camera = pointer(pointer(mover + 0x10)); // SM2: +10 (SM2VR)
    const auto local = record.load();
    const auto player = pointer(local);
    float before[16]{}, after[16]{}, body[16]{};
    const bool valid = livePlayer() && pointer(targetAddress + 0x10) == local &&
                       (vt == base + followMover || vt == base + combatMover) && camera != player &&
                       transform(camera, before) && transform(player, body);
    const auto command = current();
    bool wrote = false;
    if (valid && writable && (command.modes & Mode::cameraOffset)) {
        float position[3]{};
        for (int i = 0; i < 3; ++i)
            position[i] = before[12 + i] + before[i] * command.offset[0] + before[4 + i] * command.offset[1] +
                          before[8 + i] * command.offset[2];
        // Native setter maintains actor dirty flags and change stamp.
        reinterpret_cast<SetPosition>(base + 0x2cfc900)(reinterpret_cast<void*>(camera), position);
        wrote = true;
    }
    if (valid)
        transform(camera, after);
    if (TryAcquireSRWLockExclusive(&telemetry)) {
        auto& d = SpidyBridgeData;
        InterlockedIncrement64(&d.sequence);
        ++d.cameraCalls;
        d.matched += valid;
        d.cameraWrites += wrote;
        d.inputFrames = inputFrames.load();
        d.digitalQueries = digitalQueries.load();
        d.analogQueries = analogQueries.load();
        d.inputServed = inputServed.load();
        d.inputSelf = inputSelf.load();
        for (int i = 0; i < 8; ++i)
            d.queriedKeys[i] = queriedKeys[i].load();
        LARGE_INTEGER qpc{};
        QueryPerformanceCounter(&qpc);
        d.qpc = qpc.QuadPart;
        d.thread = GetCurrentThreadId();
        d.mover = mover;
        d.target = targetAddress;
        d.caller = caller;
        d.cameraTransform = camera;
        d.playerTransform = player;
        d.serial = command.serial;
        d.expired = expired.load();
        std::memcpy(d.before, before, 64);
        std::memcpy(d.after, after, 64);
        std::memcpy(d.player, body, 64);
        InterlockedIncrement64(&d.sequence);
        ReleaseSRWLockExclusive(&telemetry);
    }
}
void commit(void* self, void* target) {
    const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    originalCommit(self, target);
    placed(reinterpret_cast<uintptr_t>(self), reinterpret_cast<uintptr_t>(target), caller, true);
}
void combatUpdate(void* self, void* target, float seconds, bool update, bool force) {
    const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    originalCombat(self, target, seconds, update, force);
    placed(reinterpret_cast<uintptr_t>(self), reinterpret_cast<uintptr_t>(target), caller, false);
}
void clearInput(void* self, uint64_t keepHeld) {
    originalClear(self, keepHeld);
    inputSelf = reinterpret_cast<uintptr_t>(self);
    ++inputFrames;
    const auto command = current();
    const uint32_t desired = (command.modes & Mode::input) && (keepHeld & 0xff) && livePlayer() ? command.keys : 0;
    // Native code clears transition bits once per poll. Our transitions use the
    // same boundary, so multiple bindings see one coherent press/release frame.
    for (unsigned i = 0; i < std::size(keyCodes); ++i) {
        const bool previous = (keyStates[i].load() & 4) != 0;
        const bool next = (desired & (1u << i)) != 0;
        keyStates[i] = next ? (previous ? 4u : 6u) : (previous ? 9u : 0u);
    }
}
uint32_t digital(void* self, uint32_t code, uint32_t user) {
    const auto stock = originalDigital(self, code, user);
    ++digitalQueries;
    if (code < 256)
        queriedKeys[code / 32].fetch_or(1u << (code % 32));
    if (user != 0 || !enabled.load())
        return stock;
    for (unsigned i = 0; i < std::size(keyCodes); ++i)
        if (code == keyCodes[i]) {
            const auto state = keyStates[i].load();
            if (state) {
                ++inputServed;
                return (stock & 4) ? stock : state;
            }
        }
    return stock;
}
float analog(void* self, uint32_t code, uint32_t user) {
    const float stock = originalAnalog(self, code, user);
    ++analogQueries;
    if (user != 0 || !enabled.load())
        return stock;
    for (unsigned i = 0; i < std::size(keyCodes); ++i)
        if (code == keyCodes[i] && (keyStates[i].load() & 4)) {
            ++inputServed;
            return 1.f;
        }
    return stock;
}
bool matches(uintptr_t rva, const unsigned char* bytes, size_t size) {
    unsigned char actual[32]{};
    return size <= sizeof(actual) && read(base + rva, actual, size) && !std::memcmp(actual, bytes, size);
}
} // namespace

extern "C" __declspec(dllexport) DWORD WINAPI SpidySubmit(void* value) {
    Control next{};
    if (!read(reinterpret_cast<uintptr_t>(value), &next, sizeof(next)) || next.magic != controlMagic ||
        next.protocol != version || next.bytes != sizeof(next) || (next.modes & ~3u) || next.leaseMs > 500 ||
        (next.keys >> std::size(keyCodes)))
        return 2001;
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(next.offset[i]) || std::abs(next.offset[i]) > 2)
            return 2002;
    AcquireSRWLockExclusive(&controls);
    if (next.serial <= control.serial) {
        ReleaseSRWLockExclusive(&controls);
        return 2003;
    }
    control = next;
    deadline = GetTickCount64() + next.leaseMs;
    ReleaseSRWLockExclusive(&controls);
    return 0;
}

// The in-process VR thread needs a coherent gameplay sample. External probes
// use the seqlock; this local API uses the writer's lock and avoids torn poses.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyBridgeSample(void* output) {
    if(!output)return 2101;
    AcquireSRWLockShared(&telemetry);
    std::memcpy(output,&SpidyBridgeData,sizeof(SpidyBridgeData));
    ReleaseSRWLockShared(&telemetry);
    return 0;
}

extern "C" __declspec(dllexport) DWORD WINAPI SpidyStart(void* value) {
    AcquireSRWLockExclusive(&lifecycle);
    DWORD result = 0;
    do {
        if (enabled.load())
            break;
        Config config{};
        base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if (!GetModuleHandleW(L"Spider-Man2.exe") ||
            !read(reinterpret_cast<uintptr_t>(value), &config, sizeof(config)) ||
            config.magic != configMagic || config.protocol != version || config.bytes != sizeof(config) ||
            config.pid != GetCurrentProcessId() || config.imageBase != base) {
            result = 1001;
            break;
        }
        // Without a player (the game is still in its menus) the bridge waits
        // for SpidyRetarget; until then no camera commit matches.
        if ((config.hero || config.actorRecord) && !isPlayer(config.hero, config.actorRecord)) {
            result = 1002;
            break;
        }
        hero = config.hero;
        record = config.actorRecord;
        // Spider-Man 2 (2.810.0.0) entry bytes, as SM2VR 0.0.27 checks them.
        const unsigned char commitBytes[] = {0x48, 0x83, 0xec, 0x38, 0xc5, 0xfb, 0x10, 0x81, 0x54, 0x03, 0x00, 0x00};
        const unsigned char queryBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xec, 0x20};
        const unsigned char analogBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x48, 0x83, 0xec, 0x20};
        const unsigned char clearBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10};
        const unsigned char setterBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec};
        // test rdx, rdx; je (no target); vmovss [rsp+18h], xmm2; push rbp
        const unsigned char combatBytes[] = {0x48, 0x85, 0xd2, 0x0f, 0x84, 0x6a, 0x06, 0x00, 0x00, 0xc5, 0xfa, 0x11, 0x54, 0x24, 0x18, 0x55};
        // The combat camera's update is slot 23 of its vtable in Spider-Man 2 (21 before).
        if (!matches(0x334d950, commitBytes, sizeof(commitBytes)) ||
            !matches(0x31c5060, queryBytes, sizeof(queryBytes)) ||
            !matches(0x31c4e40, analogBytes, sizeof(analogBytes)) ||
            !matches(0x31c8c20, clearBytes, sizeof(clearBytes)) ||
            !matches(0x2cfc900, setterBytes, sizeof(setterBytes)) ||
            !matches(combatUpdateRva, combatBytes, sizeof(combatBytes)) ||
            pointer(base + combatMover + 23 * 8) != base + combatUpdateRva) {
            result = 1003;
            break;
        }
        if (!created) {
            auto status = MH_Initialize();
            if (status != MH_OK) {
                result = 1100 + status;
                break;
            }
            // SM2: hook i's MinHook failures read 1200 + 20 * i + MH_STATUS (create) and 1300 + 20 * i +
            // MH_STATUS (enable), i in the order below: 0 commit, 1 clear, 2 digital, 3 analog, 4 combat
            // camera (tools/run_game_vr.py names them). Hook 0 keeps the old 1200 + status.
            // The commit entry (sub rsp then vmovsd) and the combat one (test, je, then vmovss at +9, which
            // MinHook decodes before it stops) are AVX (VEX): MinHook 1.3.4's HDE64 refuses both (1208 and
            // 1288). The MinHook pinned in tools/bootstrap.ps1 (upstream 8af6b4a) decodes VEX, as the HDE64
            // linked into SM2VR 0.0.27's spidy_bridge.dll does; it hooks these same five entries.
            const uintptr_t rvas[] = {0x334d950, 0x31c8c20, 0x31c5060, 0x31c4e40, combatUpdateRva};
            void* detours[] = {reinterpret_cast<void*>(commit), reinterpret_cast<void*>(clearInput),
                               reinterpret_cast<void*>(digital), reinterpret_cast<void*>(analog),
                               reinterpret_cast<void*>(combatUpdate)};
            void** originals[] = {
                reinterpret_cast<void**>(&originalCommit), reinterpret_cast<void**>(&originalClear),
                reinterpret_cast<void**>(&originalDigital), reinterpret_cast<void**>(&originalAnalog),
                reinterpret_cast<void**>(&originalCombat)};
            for (unsigned i = 0; i < hooks.size(); ++i) {
                hooks[i] = reinterpret_cast<void*>(base + rvas[i]);
                status = MH_CreateHook(hooks[i], detours[i], originals[i]);
                if (status != MH_OK) {
                    result = 1200 + 20 * static_cast<int>(i) + status;
                    break;
                }
            }
            if (result) {
                MH_Uninitialize();
                hooks = {};
                break;
            }
            created = true;
        }
        enabled = true;
        for (unsigned i = 0; i < hooks.size(); ++i) {
            auto status = MH_EnableHook(hooks[i]);
            if (status != MH_OK) {
                result = 1300 + 20 * static_cast<int>(i) + status;
                break;
            }
        }
        if (result) {
            enabled = false;
            for (auto hook : hooks)
                MH_DisableHook(hook);
        }
    } while (false);
    SpidyBridgeData.state = result ? 3 : 1;
    SpidyBridgeData.error = result;
    ReleaseSRWLockExclusive(&lifecycle);
    return result;
}
// Follows the local player to another actor; zero detaches until there is
// one again. Same Config as SpidyStart.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyRetarget(void* value) {
    Config next{};
    if (!read(reinterpret_cast<uintptr_t>(value), &next, sizeof(next)) || next.magic != configMagic ||
        next.protocol != version || next.bytes != sizeof(next) || next.pid != GetCurrentProcessId() ||
        !base || next.imageBase != base || !next.hero != !next.actorRecord)
        return 1001;
    if (next.hero && !isPlayer(next.hero, next.actorRecord))
        return 1002;
    AcquireSRWLockExclusive(&lifecycle);
    // A commit in between sees no player rather than a mix of two.
    record = 0;
    hero = next.hero;
    record = next.actorRecord;
    ReleaseSRWLockExclusive(&lifecycle);
    return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyStop(void*) {
    AcquireSRWLockExclusive(&lifecycle);
    enabled = false;
    for (auto& state : keyStates)
        state = 0;
    DWORD result{};
    if (created)
        for (auto hook : hooks) {
            const auto status = MH_DisableHook(hook);
            if (status != MH_OK && status != MH_ERROR_DISABLED)
                result = 1400 + status;
        }
    SpidyBridgeData.state = result ? 3 : 2;
    SpidyBridgeData.error = result;
    ReleaseSRWLockExclusive(&lifecycle);
    return result; // Resident until exit: never unmap an in-flight trampoline.
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(instance);
    return TRUE;
}
