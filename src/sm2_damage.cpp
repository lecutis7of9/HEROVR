// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// Version 0.0.5: the damage path's runtime (include/spidy/sm2_damage.hpp). DAMAGE TEST only
// (experimental, OFF by default); every touch of the game's memory SEH-guarded; a fault
// closes the path for the session (never a crash, never flatscreen).
#include "spidy/sm2_damage.hpp"
#include <atomic>
#include <cmath>
#include <cstring>
#include <windows.h>

namespace spidy::sm2_damage {
namespace {
uintptr_t base{};
std::atomic<uint32_t> statusNow{off}, codeNow{codeNoBase};
std::atomic<bool> wanted{};
std::atomic<uint64_t> nQueued{}, nSent{}, nNotSlot{}, nFull{}, nFaults{}, nRefusedOff{}, nMelee{}, meleeAt{},
    sentAt{}, lastRecord{};
std::atomic<uint32_t> lastHandle{};
std::atomic<uint64_t> hero{};
struct Pending {
    uint64_t record;
    Blow blow;
    float point[3], dir[3];
};
constexpr uint32_t most = 16;
SRWLOCK lock = SRWLOCK_INIT;
Pending pending[most];
uint32_t pendingCount{};

bool readSafe(uintptr_t p, void* out, size_t n) {
    __try {
        if (p < 0x10000)
            return false;
        std::memcpy(out, reinterpret_cast<const void*>(p), n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// The record's handle when it is a live actor-table slot (0: not one).
uint32_t slotHandle(uint64_t record) {
    if (!base || !record)
        return 0;
    uintptr_t table{};
    uint32_t count{};
    uint16_t generation{};
    uint32_t index{};
    if (!readSafe(base + actorTableRva, &table, 8) || !readSafe(base + actorCountRva, &count, 4) ||
        !readSafe(static_cast<uintptr_t>(record) + 8, &generation, 2) ||
        !readSafe(static_cast<uintptr_t>(record) + 0xc, &index, 4))
        return 0;
    return isSlot(record, table, count, generation, index) ? handleOf(generation, index) : 0;
}
using Direct = unsigned char*(__fastcall*)(void*, const uint32_t*, const float*, const float*, const float*);
// One request; 0 sent, 1 full (the spare: never written), 2 fault.
int sendOne(uint32_t handle, const Pending& p) {
    __try {
        const auto system = base + systemRva;
        const auto direct = reinterpret_cast<Direct>(base + directRva);
        const float normal[3] = {-p.dir[0], -p.dir[1], -p.dir[2]};
        unsigned char* request = direct(reinterpret_cast<void*>(system), &handle, p.dir, p.point, normal);
        if (!poolRequest(reinterpret_cast<uint64_t>(request), system))
            return 1;
        fill(request, p.blow);
        return 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 2;
    }
}
} // namespace

uint32_t start(uintptr_t imageBase) {
    base = imageBase;
    uint32_t code = noCode;
    unsigned char bytes[26]{};
    uintptr_t vt{};
    if (!imageBase)
        code = codeNoBase;
    else if (!readSafe(imageBase + directRva, bytes, sizeof(directPrologue)) ||
             std::memcmp(bytes, directPrologue, sizeof(directPrologue)) != 0)
        code = codePrologue;
    else if (!readSafe(imageBase + allocRva, bytes, sizeof(allocPrologue)) ||
             std::memcmp(bytes, allocPrologue, sizeof(allocPrologue)) != 0)
        code = codeAlloc;
    else if (!readSafe(imageBase + systemRva, &vt, 8) || vt != imageBase + systemVtableRva)
        code = codeVtable;
    codeNow = code;
    statusNow = code ? refused : (wanted.load() ? ready : off);
    return code;
}
void setEnabled(bool on) {
    wanted = on;
    if (codeNow.load() == noCode) {
        const uint32_t s = statusNow.load();
        if (!on)
            statusNow = off;
        else if (s == off)
            statusNow = ready;
    }
}
bool enabled() { return wanted.load() && codeNow.load() == noCode; }
void setHero(uint64_t heroRecord) { hero = heroRecord; }
bool queue(uint64_t record, const Blow& b) {
    if (!enabled()) {
        ++nRefusedOff;
        return false;
    }
    AcquireSRWLockExclusive(&lock);
    const bool room = pendingCount < most;
    if (room) {
        Pending& p = pending[pendingCount++];
        p.record = record;
        p.blow = b;
    }
    ReleaseSRWLockExclusive(&lock);
    if (room)
        ++nQueued;
    return room;
}
void service() {
    if (!pendingCount) // a plain read: the next call sees it
        return;
    Pending now[4]{};
    uint32_t n = 0;
    AcquireSRWLockExclusive(&lock);
    while (pendingCount && n < 4) {
        now[n++] = pending[0];
        std::memmove(pending, pending + 1, (pendingCount - 1) * sizeof(Pending));
        --pendingCount;
    }
    ReleaseSRWLockExclusive(&lock);
    for (uint32_t i = 0; i < n; ++i) {
        if (!enabled())
            return;
        const uint32_t handle = slotHandle(now[i].record);
        if (!handle) {
            ++nNotSlot;
            continue;
        }
        // The point: the victim's transform position (+0x30), as the melee site; the hero's too.
        float victimAt[3]{}, heroAt[3]{};
        uintptr_t transform{};
        if (!readSafe(static_cast<uintptr_t>(now[i].record), &transform, 8) || !readSafe(transform + 0x30, victimAt, 12)) {
            ++nNotSlot;
            continue;
        }
        if (!std::isfinite(victimAt[0]) || !std::isfinite(victimAt[1]) || !std::isfinite(victimAt[2]) ||
            std::abs(victimAt[0]) > 1e6f || std::abs(victimAt[1]) > 1e6f || std::abs(victimAt[2]) > 1e6f) {
            ++nNotSlot;
            continue;
        }
        const uint64_t me = hero.load();
        uintptr_t heroTransform{};
        if (!me || !readSafe(static_cast<uintptr_t>(me), &heroTransform, 8) || !readSafe(heroTransform + 0x30, heroAt, 12))
            std::memcpy(heroAt, victimAt, sizeof(heroAt));
        std::memcpy(now[i].point, victimAt, sizeof(victimAt));
        if (now[i].blow.toward) // 0.0.74 test 10 GRAPPLE YANK (CANDIDATE): from him toward the hero
            directionOf(victimAt, heroAt, now[i].dir);
        else
            directionOf(heroAt, victimAt, now[i].dir);
        if (!now[i].blow.damager && me)
            now[i].blow.damager = slotHandle(me);
        const int r = sendOne(handle, now[i]);
        if (r == 2) {
            ++nFaults;
            codeNow = codeFault;
            statusNow = refused;
            return;
        }
        if (r == 1) {
            ++nFull;
            continue;
        }
        ++nSent;
        sentAt = GetTickCount64();
        lastRecord = now[i].record;
        lastHandle = handle;
        uint32_t s = statusNow.load();
        if (s == ready)
            statusNow.compare_exchange_strong(s, sent);
    }
}
void noteMelee() {
    ++nMelee;
    meleeAt = GetTickCount64();
}
void markConfirmed() {
    if (codeNow.load() == noCode && statusNow.load() == sent)
        statusNow = confirmed;
}
Stats stats() {
    Stats s;
    s.status = statusNow.load();
    s.code = codeNow.load();
    s.queued = nQueued.load();
    s.sent = nSent.load();
    s.notSlot = nNotSlot.load();
    s.full = nFull.load();
    s.faults = nFaults.load();
    s.refusedOff = nRefusedOff.load();
    s.meleePresses = nMelee.load();
    s.lastMeleeMs = meleeAt.load();
    s.lastSentMs = sentAt.load();
    s.lastRecord = lastRecord.load();
    s.lastHandle = lastHandle.load();
    return s;
}
} // namespace spidy::sm2_damage
