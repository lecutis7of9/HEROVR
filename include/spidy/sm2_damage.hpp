#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
//
// Version 0.0.5 (Update 38.9): THE DAMAGE PATH - the same logic as a punch (owner, October 25).
//
// The game's punches, shots and knockbacks all queue a request with its DamageSystem
// (DirectDamage), as SpidyVR's Spider-Man Remastered did; in Spider-Man 2 every field bit is
// one higher. Static reading of Spider-Man2.exe (base 0x140000000), October 25:
//   DamageSystem instance   RVA 0xa25b580, its first qword the vtable RVA 0x85c0b40.
//   DirectDamage            RVA 0x3412ec0 (rcx system, rdx const uint32_t* victim handle,
//                           r8 Vec3* direction, r9 Vec3* point, [rsp+20h] Vec3* normal); it takes
//                           a pool slot (0x3412da0: lock xadd, any thread) and answers the request
//                           (slot + 0x68), or a scratch spare (system + 0x127860 and on) when full.
//   Request fields          each with a bit ORed into both +8 and +0x18 (the "set" masks):
//                           damager handle +0x138 bit 9, type +0x13c bit 10, amount (float)
//                           +0x144 bit 12, knockback +0x150 bit 15, knockback amount (float)
//                           +0x154 bit 16, flags +0x160 bit 19, hash +0x1cc bit 27.
//   The melee site 0x8558c7 (a hero blow): its victim handle built from the actor itself -
//   (word at +8 & 0x7ff) << 20 | (dword at +0xc & 0xfffff) - the actor table's slot (table
//   pointer RVA 0xc2d3d90, 0xc0 bytes a slot, count RVA 0xc2d3dac; resolver 0x28ab5e0), its
//   point the actor's transform position (+0x30), amount bit 0x1000, knockback amount 100.
//   No commit call after the fill: the system reads the pool on its own update.
// STATUS: STATIC-STRONG (read from the code), CANDIDATE live until a headset run shows an
// enemy's hit points fall after the mod's own request (the V005 record's write state 2).
// Before every request: the DirectDamage prologue and the system's vtable are checked, the
// victim must be a live actor-table slot (aligned, its index matching), the answer must be
// a pool slot (never the spare), and every touch is SEH-guarded - a fault turns it off.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace spidy::sm2_damage {

inline constexpr uint64_t systemRva = 0xa25b580, systemVtableRva = 0x85c0b40, directRva = 0x3412ec0,
                          allocRva = 0x3412da0, actorTableRva = 0xc2d3d90, actorCountRva = 0xc2d3dac;
inline constexpr uint32_t actorSlotBytes = 0xc0, spareAt = 0x127860, poolEnd = 0x127854;
inline constexpr unsigned char directPrologue[26] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24,
                                                     0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x48, 0x89, 0x7c,
                                                     0x24, 0x20, 0x41, 0x56, 0x48, 0x83, 0xec, 0x30};
inline constexpr unsigned char allocPrologue[11] = {0x48, 0x83, 0xec, 0x28, 0x44, 0x8b, 0x81, 0x54, 0x78, 0x12, 0x00};
// Request field offsets (from the request pointer DirectDamage answers) and their bits.
inline constexpr uint32_t setA = 0x8, setB = 0x18, damagerAt = 0x138, typeAt = 0x13c, amountAt = 0x144,
                          knockAt = 0x150, knockAmountAt = 0x154, flagsAt = 0x160, requestBytes = 0x1d0;
inline constexpr uint32_t damagerBit = 1u << 9, typeBit = 1u << 10, amountBit = 1u << 12, knockBit = 1u << 15,
                          knockAmountBit = 1u << 16, flagsBit = 1u << 19;
inline constexpr uint32_t meleeType = 7; // the melee site's type

// One blow the mod asks for.
struct Blow {
    float amount{};          // hit points
    uint32_t knockback{};    // the game's knockback kind (0: none)
    float knockAmount{};     // its strength (the melee site's 100)
    uint32_t damager{};      // the hero's handle (0: none)
    uint32_t type = meleeType;
};
// A handle from an actor slot's generation word and index (the melee site's own build).
inline uint32_t handleOf(uint16_t generation, uint32_t index) {
    return (static_cast<uint32_t>(generation) & 0x7ffu) << 20 | (index & 0xfffffu);
}
// A record is a live actor slot: inside the table, on a slot's start, its index its own, a
// generation (0: none - the resolver's own refusal).
inline bool isSlot(uint64_t record, uint64_t table, uint32_t count, uint16_t generation, uint32_t index) {
    if (!table || record < table || (generation & 0x7ff) == 0)
        return false;
    const uint64_t off = record - table;
    return off % actorSlotBytes == 0 && off / actorSlotBytes < count && off / actorSlotBytes == (index & 0xfffff);
}
// Fill a request (requestBytes bytes from DirectDamage's answer) as the melee site does.
// Pure (tests): only the blow's fields and the two masks change.
inline void fill(unsigned char* request, const Blow& b) {
    const auto put32 = [&](uint32_t at, uint32_t v) { std::memcpy(request + at, &v, 4); };
    const auto putF = [&](uint32_t at, float v) { std::memcpy(request + at, &v, 4); };
    const auto orMask = [&](uint32_t bits) {
        for (const uint32_t at : {setA, setB}) {
            uint32_t m{};
            std::memcpy(&m, request + at, 4);
            m |= bits;
            std::memcpy(request + at, &m, 4);
        }
    };
    put32(typeAt, b.type);
    putF(amountAt, b.amount);
    uint32_t bits = typeBit | amountBit;
    if (b.knockback) {
        put32(knockAt, b.knockback);
        putF(knockAmountAt, b.knockAmount);
        bits |= knockBit | knockAmountBit;
    }
    if (b.damager) {
        put32(damagerAt, b.damager);
        bits |= damagerBit;
    }
    orMask(bits);
}
// The direction from the hero to the victim (unit; straight ahead +z when they coincide).
inline void directionOf(const float hero[3], const float victim[3], float out[3]) {
    float d[3] = {victim[0] - hero[0], victim[1] - hero[1], victim[2] - hero[2]};
    const float l2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    if (!(l2 > 1e-6f) || !(l2 < 1e12f)) {
        out[0] = 0;
        out[1] = 0;
        out[2] = 1;
        return;
    }
    const float r = std::sqrt(l2);
    out[0] = d[0] / r;
    out[1] = d[1] / r;
    out[2] = d[2] / r;
}
// The answer is one of the pool's slots, never the spare or a stranger.
inline bool poolRequest(uint64_t request, uint64_t system) {
    return system && request > system + 0x68 && request + requestBytes <= system + poolEnd;
}

// Version 0.0.5 hero blows (hero_modes.md): what each hero's hit asks for (damage CANDIDATE).
inline constexpr float thugHp = 100.f; // a normal thug's hit points (the probe reports the real ones)
inline Blow repulsorBlast() { return {35.f, 5, 100.f}; }  // ~3 blasts a thug
inline Blow chestBeam() { return {140.f, 5, 300.f}; }      // 4x a blast
inline Blow batonHit() { return {30.f, 5, 120.f}; }
inline Blow webBallTest() { return {10.f, 0, 0.f}; }       // DAMAGE TEST's web ball: small, no knockback
// The runtime status (V005 record's write state).
enum Status : uint32_t { off = 0, ready = 1, sent = 2, confirmed = 3, refused = 4 };
inline const char* statusName(uint32_t s) {
    constexpr const char* names[] = {"OFF", "READY", "SENT", "CONFIRMED", "REFUSED"};
    return s < 5 ? names[s] : "?";
}
// Refusal codes (why the path is closed).
enum Code : uint32_t {
    noCode = 0, codePrologue = 9501, codeAlloc = 9502, codeVtable = 9503, codeFault = 9504, codeNoBase = 9505
};

// The runtime (sm2_damage.cpp, Windows only).
uint32_t start(uintptr_t imageBase);       // the checks; noCode when the path is open
void setEnabled(bool on);                  // DAMAGE TEST (experimental, OFF by default)
bool enabled();
// Queue one blow at an enemy record (world_trace's id). False: off, closed or the queue full.
// Sent from the game's input thread (service): the point is the victim's own transform
// position (+0x30, as the melee site), the direction from the hero to it; the hero's handle
// (setHero) the damager.
bool queue(uint64_t record, const Blow& b);
void setHero(uint64_t heroRecord);
void service();                            // game_pad's serve: sends at most 4 a call
void noteMelee();                          // the player pressed X (the game's own punch)
void markConfirmed();                      // the DAMAGE PROBE saw our blow land (write CONFIRMED)
struct Stats {
    uint32_t status{}, code{};
    uint64_t queued{}, sent{}, notSlot{}, full{}, faults{}, refusedOff{};
    uint64_t meleePresses{};
    uint64_t lastMeleeMs{}, lastSentMs{};  // GetTickCount64 times (0: never)
    uint64_t lastRecord{};
    uint32_t lastHandle{};
};
Stats stats();

} // namespace spidy::sm2_damage
