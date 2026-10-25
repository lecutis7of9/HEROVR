// Leased velocity requests enter MoverStandard before its native body queries.
// Actor transforms, collision results, and native movement flags are never written directly.
// Owned steps also discard the native mover's duplicate gravity accumulator.
//
// SM2 VR (port of SpidyVR): every Spider-Man 2 address, check byte and struct
// offset below is taken from SM2VR 0.0.27's spidy_movement_bridge.dll
// (disassembled: start 0x2d90, live 0x1610, handleAir 0x1210, query 0x1790,
// queryDriven 0x1cd0, applyGravity 0x1010, readEvent 0x2440). Remastered value
// -> SM2 value: registry 7a44320/7a44340 -> c2d3d20/c2d3d40; MoverStandard
// vtable 4f70168 -> 85d24d8; mover record +8 -> +0x10, own handle +0x14 ->
// +0x1c, flags +0x750 -> +0x77c, collision +0x144 -> +0x154, contact +0x6ee ->
// +0x716, fall accumulator +0x6f0 -> +0x718; requested +0x11c, record position
// +0x30, air state +0x3f8/+0x3cc/+0x3a8 and the 0x800003 collision mask are
// unchanged. Air tables 38c1340/38c9090 -> 5452e08/5460fb8; air-event caller
// a7b489 -> 12dea2c. Functions: query 1fbe360 -> 35184e0, gravity 1fbda50 ->
// 3517bb0, request 1fc2e10 -> 351dc70, setTarget 1fb88d0 -> 35130c0, timestep
// 16769f0 -> 29bd6b0, air 0xa7b3a0 -> 12de950, eventRead 1f9db60 -> 350e880.
#include "spidy/native_movement.hpp"
#include <MinHook.h>
#include <atomic>
#include <cstring>
#include <intrin.h>
#include <windows.h>
using namespace spidy;
using namespace spidy::native_movement;
extern "C" {
__declspec(dllexport) Data SpidyMotionData;
__declspec(dllexport) DrivenData SpidyMotionDriven;
}
namespace {
using Method = uintptr_t (*)(void*);
using Request = uintptr_t (*)(void*, const Vec3*, const Vec3*, uint8_t);
using SetTarget = uintptr_t (*)(void*, const Vec3*);
using Timestep = float (*)(void*);
using AirEvent = uintptr_t (*)(void*, void*);
using EventRead = bool (*)(void*, int, void*, unsigned);
Method prequery{}, gravity{};
AirEvent airEvent{};
EventRead eventRead{};
Request request{};
SetTarget setTarget{};
Timestep timestep{};
Config config{};
Command command{};
void* hooks[4]{};
unsigned hookCount{};
uint32_t handle{};
std::atomic<bool> enabled{};
// Set when the module follows the player to another mover: the next step's
// achieved velocity would otherwise span the jump between the two bodies.
std::atomic<bool> restarted{};
std::atomic<unsigned> active{};
std::atomic<uint64_t> corrections{};
// Steps of the player's mover so far, and how many had run when the command
// arrived (commandApplies).
std::atomic<uint64_t> playerSteps{};
uint64_t deadline{}, commandDeadline{}, commandStep{};
SRWLOCK lifecycle = SRWLOCK_INIT, control = SRWLOCK_INIT, telemetry = SRWLOCK_INIT;
// Other actors' movers on a web (driveLock); their telemetry is under `telemetry`.
Drive drives[driveSlots]{};
uint64_t driveDeadlines[driveSlots]{};
SRWLOCK driveLock = SRWLOCK_INIT;
// A collision step is synchronous up to dispatch of its body-query job. Gravity
// correction is scoped to this exact native invocation, never another actor/thread.
struct Override {
    void* self;
    Vec3 target;
};
thread_local Override* current{};
struct AirOverride {
    Vec3 delta;
    uint64_t applied{};
};
thread_local AirOverride* currentAir{};
struct Guard {
    Guard() {
        ++active;
    }
    ~Guard() {
        --active;
    }
};
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
uintptr_t pointer(uintptr_t p) {
    uintptr_t v{};
    read(p, &v, 8);
    return v;
}
bool live() {
    const auto table = pointer(config.base + 0xc2d3d20);
    const auto index = handle & 0xfffff;
    uint32_t count{}, generation{}, own{};
    return table && read(config.base + 0xc2d3d40, &count, 4) && index < count && count <= 0x100000 &&
           read(table + index * 16 + 8, &generation, 4) && generation == (handle >> 20) && generation &&
           pointer(table + index * 16) == config.mover && pointer(config.mover) == config.base + 0x85d24d8 &&
           pointer(config.mover + 0x10) == config.record && read(config.mover + 0x1c, &own, 4) && own == handle;
}
// A registered MoverStandard on `record`, under its own handle.
bool moverLive(uintptr_t mover, uintptr_t record) {
    const auto table = pointer(config.base + 0xc2d3d20);
    uint32_t count{}, generation{}, own{};
    if (!table || !read(mover + 0x1c, &own, 4) || !read(config.base + 0xc2d3d40, &count, 4) ||
        count > 0x100000)
        return false;
    const auto index = own & 0xfffff;
    return index < count && read(table + index * 16 + 8, &generation, 4) && generation == (own >> 20) &&
           generation && pointer(table + index * 16) == mover && pointer(mover) == config.base + 0x85d24d8 &&
           pointer(mover + 0x10) == record && record;
}
Command leased() {
    Command c{};
    AcquireSRWLockShared(&control);
    const auto now = GetTickCount64();
    if (enabled && now < deadline && commandApplies(now, commandDeadline, playerSteps, commandStep))
        c = command;
    ReleaseSRWLockShared(&control);
    return c;
}
bool readEvent(void* event, int field, void* out, unsigned bytes) {
    const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    Guard guard;
    const bool result = eventRead(event, field, out, bytes);
    if (result && currentAir && caller == config.base + 0x12dea2c && bytes == sizeof(Vec3)) {
        // Only the local airborne event's displacement field. Its native handler
        // derives both velocity and gravity history from this returned value.
        std::memcpy(out, &currentAir->delta, sizeof(Vec3));
        ++currentAir->applied;
    }
    return result;
}
uintptr_t handleAir(void* self, void* event) {
    Guard guard;
    const auto address = reinterpret_cast<uintptr_t>(self);
    const auto table = pointer(address);
    if (!enabled || GetTickCount64() >= deadline || !live() || pointer(address + 0x10) != config.record ||
        (table != config.base + 0x5452e08 && table != config.base + 0x5460fb8))
        return airEvent(self, event);
    const auto c = leased();
    const float dt = timestep(self);
    uint32_t flags{}, collision{};
    const bool drive = config.syncAirVelocity && c.enabled && !currentAir &&
                       read(config.mover + 0x77c, &flags, 4) && !(flags & 0x80000000u) &&
                       read(config.mover + 0x154, &collision, 4) && collisionEnabled(collision) &&
                       drivable(dt);
    AirOverride override{stepDelta(c.velocity, dt)};
    auto* previous = currentAir;
    if (drive)
        currentAir = &override;
    const auto result = airEvent(self, event);
    currentAir = previous;
    Vec3 velocity{};
    float vertical{}, acceleration{};
    if (read(address + 0x3f8, &velocity, 12) && read(address + 0x3cc, &vertical, 4) &&
        read(address + 0x3a8, &acceleration, 4)) {
        AcquireSRWLockExclusive(&telemetry);
        auto& d = SpidyMotionData;
        InterlockedIncrement64(&d.sequence);
        ++d.airEvents;
        d.airOverrides += override.applied;
        d.airState = table;
        d.airVelocity = velocity;
        d.airVertical = vertical;
        d.airGravity = acceleration;
        d.airDt = dt;
        InterlockedIncrement64(&d.sequence);
        ReleaseSRWLockExclusive(&telemetry);
    }
    return result;
}
uintptr_t applyGravity(void* self) {
    Guard guard;
    const auto result = gravity(self);
    if (current && current->self == self) {
        // 1fbda50 integrates fall speed at +6f0 even while our target replaces
        // its displacement. The solver already includes gravity. Keeping both
        // integrators caused a sudden dive when the movement lease ended.
        const float zero{};
        std::memcpy(static_cast<uint8_t*>(self) + 0x718, &zero, sizeof(zero));
        setTarget(self, &current->target);
        ++corrections;
    }
    return result;
}
// A step of another actor's mover: driven when a web holds it.
uintptr_t queryDriven(void* self) {
    const auto mover = reinterpret_cast<uintptr_t>(self);
    unsigned slot = driveSlots;
    Drive c{};
    bool leasedNow{};
    AcquireSRWLockShared(&driveLock);
    for (unsigned i = 0; i < driveSlots; ++i)
        if (drives[i].mover == mover) {
            slot = i;
            c = drives[i];
            leasedNow = c.enabled && GetTickCount64() < driveDeadlines[i];
            break;
        }
    ReleaseSRWLockShared(&driveLock);
    if (slot == driveSlots || !moverLive(mover, c.record))
        return prequery(self);
    Vec3 position{};
    uint32_t flags{}, collision{};
    uint8_t contact{};
    if (!read(pointer(c.record) + 0x30, &position, 12) || !finite(position) ||
        !read(mover + 0x77c, &flags, 4) || !read(mover + 0x154, &collision, 4) ||
        !read(mover + 0x716, &contact, 1))
        return prequery(self);
    const float dt = timestep(self);
    const bool drive = leasedNow && (collisionEnabled(collision) || (c.options & 1)) &&
                       !(flags & 0x80000000u) && drivable(dt) && !current;
    Override step{self, position};
    Override* previous = current;
    if (drive) {
        const Vec3 delta = stepDelta(c.velocity, dt), keepDirection{};
        request(self, &delta, &keepDirection, 0);
        step.target = position + delta;
        current = &step;
    }
    const auto result = prequery(self);
    current = previous;
    AcquireSRWLockExclusive(&telemetry);
    auto& d = SpidyMotionDriven;
    InterlockedIncrement64(&d.sequence);
    auto& m = d.movers[slot];
    if (m.mover != mover || m.record != c.record)
        m = {mover, c.record};
    m.achievedVelocity = m.steps && m.dt > 0 ? (position - m.position) / m.dt : Vec3{};
    m.position = position;
    m.dt = dt;
    ++m.steps;
    m.controlled += drive;
    m.serial = drive ? c.serial : 0;
    m.grounded = groundedContact(contact);
    m.contact = contact;
    m.moverFlags = flags;
    m.collisionFlags = collision;
    InterlockedIncrement64(&d.sequence);
    ReleaseSRWLockExclusive(&telemetry);
    return result;
}
uintptr_t query(void* self) {
    Guard guard;
    if (!enabled || GetTickCount64() >= deadline)
        return prequery(self);
    if (reinterpret_cast<uintptr_t>(self) != config.mover)
        return queryDriven(self);
    if (!live())
        return prequery(self);
    Vec3 position{};
    uint32_t flags{}, collision{};
    uint8_t contact{};
    if (!read(pointer(config.record) + 0x30, &position, 12) || !finite(position) ||
        !read(config.mover + 0x77c, &flags, 4) || !read(config.mover + 0x154, &collision, 4) ||
        !read(config.mover + 0x716, &contact, 1))
        return prequery(self);
    const float dt = timestep(self);
    const auto c = leased();
    const bool drive = c.enabled && collisionEnabled(collision) && !(flags & 0x80000000u) && drivable(dt) &&
                       !current;
    Override step{self, position};
    Override* previous = current;
    if (drive) {
        const Vec3 delta = stepDelta(c.velocity, dt), keepDirection{};
        // This verified native function sets start/target and prepares its own bookkeeping.
        request(self, &delta, &keepDirection, 0);
        step.target = position + delta;
        current = &step;
    }
    const auto result = prequery(self);
    current = previous;
    ++playerSteps;
    Vec3 requested{};
    read(config.mover + 0x11c, &requested, 12);
    LARGE_INTEGER qpc{};
    QueryPerformanceCounter(&qpc);
    AcquireSRWLockExclusive(&telemetry);
    auto& d = SpidyMotionData;
    InterlockedIncrement64(&d.sequence);
    // The previous step's collision result is available at the next prequery.
    d.achievedVelocity = !restarted.exchange(false) && d.steps && d.dt > 0 ? (position - d.position) / d.dt : Vec3{};
    d.position = position;
    d.requested = requested;
    d.dt = dt;
    d.moverFlags = flags;
    d.collisionFlags = collision;
    d.grounded = groundedContact(contact);
    d.contact = contact;
    d.qpc = qpc.QuadPart;
    ++d.steps;
    d.controlled += drive;
    d.serial = drive ? c.serial : 0;
    d.gravityCorrections = corrections;
    d.thread = GetCurrentThreadId();
    d.status = drive ? 2 : 1;
    InterlockedIncrement64(&d.sequence);
    ReleaseSRWLockExclusive(&telemetry);
    return result;
}
bool signature(uintptr_t rva, const uint8_t* expected, size_t n) {
    uint8_t bytes[16]{};
    return n <= sizeof(bytes) && read(config.base + rva, bytes, n) && !std::memcmp(bytes, expected, n);
}
DWORD disable() {
    enabled = false;
    DWORD result{};
    for (unsigned i = 0; i < hookCount; ++i) {
        const auto s = MH_DisableHook(hooks[i]);
        if (s != MH_OK && s != MH_ERROR_DISABLED)
            result = 1400 + s;
    }
    const auto until = GetTickCount64() + 2000;
    while (active && GetTickCount64() < until)
        Sleep(1);
    return active ? 1501 : result;
}
} // namespace
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMotionStart(void* input) {
    AcquireSRWLockExclusive(&lifecycle);
    DWORD result{};
    do {
        // Running already: a second start is refused. Stopped, the hooks are
        // still there, disabled, and a start takes them up again.
        if (hookCount && enabled) {
            result = 1000;
            break;
        }
        if (!read(reinterpret_cast<uintptr_t>(input), &config, sizeof(config)) ||
            config.magic != 0x534d5643 || config.version != 2 || config.bytes != sizeof(config) ||
            config.pid != GetCurrentProcessId() ||
            config.base != reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr)) ||
            !GetModuleHandleW(L"Spider-Man2.exe") || (config.durationMs && config.durationMs < 1000) ||
            config.durationMs > 30000 || !std::isfinite(config.maxSpeed) || config.maxSpeed <= 0 ||
            config.maxSpeed > motionCeiling || config.syncAirVelocity > 1 || config.reserved) {
            result = 1001;
            break;
        }
        if (!read(config.mover + 0x1c, &handle, 4) || !live()) {
            result = 1002;
            break;
        }
        // SM2VR 0.0.27 spidy_movement_bridge.dll start (0x2d90): the same seven
        // checks, these bytes. A mismatch (another game build, another mod)
        // leaves swinging off; VR carries on (game_xr reports 1003).
        constexpr uint8_t a[] = {0x40, 0x55, 0x53, 0x48, 0x8d, 0x6c, 0x24, 0xb1,
                                 0x48, 0x81, 0xec, 0xc8, 0,    0,    0};
        constexpr uint8_t b[] = {0x40, 0x53, 0x48, 0x81, 0xec, 0xa0, 0, 0, 0};
        constexpr uint8_t c[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec, 0x30};
        constexpr uint8_t d[] = {0xc5, 0xfa, 0x6f, 0x1d, 0x68, 0x14, 0xdf, 0x01};
        constexpr uint8_t e[] = {0xc5, 0xfb, 0x10, 0x05, 0xb0, 0x60, 0x96, 0x09, 0xc5, 0xf9, 0x5a, 0xc0};
        constexpr uint8_t f[] = {0x40, 0x53, 0x56, 0x57, 0x48, 0x83, 0xec, 0x60, 0x48, 0x8b, 0x32};
        constexpr uint8_t g[] = {0x48, 0x83, 0xec, 0x28, 0x48, 0x63, 0xc2, 0x4d, 0x8b, 0xd0};
        if (!signature(0x35184e0, a, sizeof(a)) || !signature(0x3517bb0, b, sizeof(b)) ||
            !signature(0x351dc70, c, sizeof(c)) || !signature(0x35130c0, d, sizeof(d)) ||
            !signature(0x29bd6b0, e, sizeof(e)) || !signature(0x12de950, f, sizeof(f)) ||
            !signature(0x350e880, g, sizeof(g))) {
            result = 1003;
            break;
        }
        const unsigned wanted = config.syncAirVelocity ? 4u : 2u;
        if (hookCount && hookCount != wanted) {
            result = 1000;
            break;
        }
        auto s = MH_OK;
        if (!hookCount) {
            s = MH_Initialize();
            if (s != MH_OK) {
                result = 1100 + s;
                break;
            }
            const uintptr_t rvas[] = {0x35184e0, 0x3517bb0, 0x12de950, 0x350e880};
            void* detours[] = {reinterpret_cast<void*>(query), reinterpret_cast<void*>(applyGravity),
                               reinterpret_cast<void*>(handleAir), reinterpret_cast<void*>(readEvent)};
            void** originals[] = {reinterpret_cast<void**>(&prequery), reinterpret_cast<void**>(&gravity),
                                  reinterpret_cast<void**>(&airEvent), reinterpret_cast<void**>(&eventRead)};
            for (unsigned i = 0; i < wanted; ++i) {
                hooks[i] = reinterpret_cast<void*>(config.base + rvas[i]);
                s = MH_CreateHook(hooks[i], detours[i], originals[i]);
                if (s != MH_OK) {
                    // 1200 + 20 * hook + status, as the game bridge (hook 0
                    // keeps 1200 + status): 0 query, 1 gravity, 2 air, 3 event.
                    result = 1200 + 20 * i + s;
                    break;
                }
                ++hookCount;
            }
            if (result)
                break;
        }
        request = reinterpret_cast<Request>(config.base + 0x351dc70);
        setTarget = reinterpret_cast<SetTarget>(config.base + 0x35130c0);
        timestep = reinterpret_cast<Timestep>(config.base + 0x29bd6b0);
        deadline = config.durationMs ? GetTickCount64() + config.durationMs : UINT64_MAX;
        enabled = true;
        for (unsigned i = 0; i < hookCount; ++i) {
            s = MH_EnableHook(hooks[i]);
            if (s != MH_OK) {
                result = 1300 + 20 * i + s;
                break;
            }
        }
    } while (false);
    if (result && hookCount)
        disable();
    ReleaseSRWLockExclusive(&lifecycle);
    return result;
}
// Follows the local player to another actor's mover (a loaded save, a
// respawn, a character switch); zeros detach until there is one again. Only
// record and mover of the Config are used.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMotionRetarget(void* input) {
    Config next{};
    if (!read(reinterpret_cast<uintptr_t>(input), &next, sizeof(next)) || next.magic != 0x534d5643 ||
        next.version != 2 || next.bytes != sizeof(next) || next.pid != GetCurrentProcessId() ||
        next.base != config.base || !next.mover != !next.record)
        return 1001;
    AcquireSRWLockExclusive(&lifecycle);
    DWORD result{};
    uint32_t nextHandle{};
    if (!hookCount) {
        result = 1004;
    } else if (next.mover && !read(next.mover + 0x1c, &nextHandle, 4)) {
        result = 1002;
    } else {
        // Hooks match their mover first: with none, every step runs stock while
        // record and handle change, and a step never sees half of each player.
        InterlockedExchange64(reinterpret_cast<volatile LONG64*>(&config.mover), 0);
        AcquireSRWLockExclusive(&control);
        command.enabled = 0;
        ReleaseSRWLockExclusive(&control);
        config.record = next.record;
        handle = nextHandle;
        restarted = true;
        InterlockedExchange64(reinterpret_cast<volatile LONG64*>(&config.mover), static_cast<LONG64>(next.mover));
        if (next.mover && !live()) {
            InterlockedExchange64(reinterpret_cast<volatile LONG64*>(&config.mover), 0);
            result = 1002;
        }
    }
    ReleaseSRWLockExclusive(&lifecycle);
    return result;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMotionSubmit(void* input) {
    Command c{};
    if (!read(reinterpret_cast<uintptr_t>(input), &c, sizeof(c)) || c.magic != 0x534d564d || c.version != 1 ||
        c.bytes != sizeof(c) || !c.serial || c.enabled > 1 || c.leaseMs > 250 || (c.enabled && !c.leaseMs) ||
        !finite(c.velocity) || length(c.velocity) > config.maxSpeed || c.reserved || c.reserved2)
        return 2001;
    AcquireSRWLockExclusive(&control);
    DWORD result{};
    if (!enabled || GetTickCount64() >= deadline)
        result = 2002;
    else if (c.serial <= command.serial)
        result = 2003;
    else {
        command = c;
        commandDeadline = GetTickCount64() + c.leaseMs;
        commandStep = playerSteps;
    }
    ReleaseSRWLockExclusive(&control);
    return result;
}
// Drives another actor's mover (a bot on a web), or lets it go (enabled 0).
// A mover takes a free slot on its first command, and keeps it with its step
// telemetry, driven or not, until another mover needs a slot.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMotionDrive(void* input) {
    Drive c{};
    if (!read(reinterpret_cast<uintptr_t>(input), &c, sizeof(c)) || c.magic != 0x534d5652 || c.version != 1 ||
        c.bytes != sizeof(c) || !c.serial || c.enabled > 1 || !c.mover || !c.record || c.leaseMs > 250 ||
        (c.enabled && !c.leaseMs) || !finite(c.velocity) || length(c.velocity) > 65 || c.options > 1 ||
        c.reserved2)
        return 2001;
    if (!enabled || GetTickCount64() >= deadline)
        return 2002;
    if (c.mover == config.mover)
        return 2005; // the player's own mover takes SpidyMotionSubmit
    AcquireSRWLockExclusive(&driveLock);
    DWORD result{};
    unsigned slot = driveSlots, free = driveSlots;
    const auto now = GetTickCount64();
    for (unsigned i = 0; i < driveSlots; ++i) {
        if (drives[i].mover == c.mover)
            slot = i;
        else if (free == driveSlots && (!drives[i].enabled || now >= driveDeadlines[i]))
            free = i;
    }
    if (slot == driveSlots)
        slot = free;
    if (slot == driveSlots)
        result = 2004;
    else if (c.serial <= drives[slot].serial && drives[slot].mover == c.mover)
        result = 2003;
    else {
        // Let go (enabled 0), a mover keeps its slot and its telemetry until
        // another mover needs the slot.
        drives[slot] = c;
        driveDeadlines[slot] = now + c.leaseMs;
    }
    ReleaseSRWLockExclusive(&driveLock);
    return result;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMotionDrivenSample(void* out) {
    AcquireSRWLockShared(&telemetry);
    DrivenData sample = SpidyMotionDriven;
    ReleaseSRWLockShared(&telemetry);
    __try {
        if (reinterpret_cast<uintptr_t>(out) < 0x10000)
            return 2201;
        std::memcpy(out, &sample, sizeof(sample));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 2201;
    }
    return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMotionStop(void*) {
    AcquireSRWLockExclusive(&lifecycle);
    const auto result = disable();
    AcquireSRWLockExclusive(&telemetry);
    auto& d = SpidyMotionData;
    InterlockedIncrement64(&d.sequence);
    d.status = 3;
    d.error = result;
    InterlockedIncrement64(&d.sequence);
    ReleaseSRWLockExclusive(&telemetry);
    ReleaseSRWLockExclusive(&lifecycle);
    return result;
}
namespace {
bool copySample(void* out, const Data& sample) {
    __try {
        if (reinterpret_cast<uintptr_t>(out) < 0x10000)
            return false;
        std::memcpy(out, &sample, sizeof(sample));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
} // namespace
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMotionSample(void* out) {
    AcquireSRWLockShared(&telemetry);
    Data sample = SpidyMotionData;
    ReleaseSRWLockShared(&telemetry);
    if (!enabled || GetTickCount64() >= deadline)
        sample.status = 0;
    return copySample(out, sample) ? 0 : 2201;
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(instance);
    return TRUE;
}
