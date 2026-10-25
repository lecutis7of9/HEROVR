#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// Update 38.4: the MIRROR PROBE - what the game would need for its own Spider-Man in the holo mirror.
//
// Silent, on from VR start, READ-ONLY: it reads game memory under SEH and counts; it never writes the
// game, calls a game function, patches code or adds a hook (the port's existing hooks hand it values).
// It stops itself if it ever costs more than `budgetUs` a frame on average, or on any fault.
// Static research: docs/mirror.md. Every address below is CONFIRMED in the exe's disassembly
// (Spider-Man2.exe 2.810.0.0) or by the port's eyes, which are made and listed through them:
// - the view manager base+0xc2ca220; Create 0x2c9c590 scans its 8 offscreen slots at +0x30 and
//   returns 0 when all are taken (0x2c9c5a9..0x2c9c5c8);
// - the view list 0x2c9c6d0: built-in views first ([manager], 0x1e20 bytes each, 64-bit active mask
//   at +8, count at +0x14), then each slot whose flags (+0x1dae4) pass `listed`; its callers cap the
//   list at 16 (0x2ceae01), 8 (0x2e15631) and 16 (0x2e3fe82);
// - a view's size +0x1da78/+0x1da7c, kind +0x1db4 (the eyes are 28), owner of a render copy +0x1dc8;
// - an instance's transform at +0, radius +0x4c, flags +0x5c (0x20: drawn off), model +0x88.
// What the suit is stored in is NOT known: the probe tallies the fields that change with the model.
// Update 38.5 PROBE v2 (version 2, the v1 fields at the same offsets, `V2` appended before
// `sequence`): milliseconds on every slot and suit event, the first 16 slot events kept for good
// (made at start or mid-game?), per-slot draws of the hero and of anything else in an offscreen slot
// (the suits menu's preview: which slot, its size, which instances, the hero's or a copy), menu
// frames (no game-view job: CANDIDATE), the port's body solves during the preview (is the
// collapsed preview ours?), and the suit tally 4x as often while a preview is up.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace spidy::mirror_probe {
inline constexpr uint32_t managerRva = 0xc2ca220;
inline constexpr uint32_t slotsAt = 0x30, slots = 8, maskAt = 8, countAt = 0x14, builtinStride = 0x1e20;
inline constexpr uint32_t flagsAt = 0x1dae4, widthAt = 0x1da78, heightAt = 0x1da7c, kindAt = 0x1db4, ownerAt = 0x1dc8;
inline constexpr uint32_t radiusAt = 0x4c, instanceFlagsAt = 0x5c, modelAt = 0x88, drawnOff = 0x20;
inline constexpr uint32_t smallestCap = 8; // 0x2e15631
inline constexpr uint32_t cameraSwitchRva = 0xc417360; // npc_fade.hpp switchRva (CANDIDATE purpose)

// 0x2c9c73e..0x2c9c74e: an offscreen view is listed (rendered) when its flags pass.
inline bool listed(uint32_t flags) { return (flags & 7) == 0 || ((flags & 6) == 0 && (flags & 0x40)); }
// 0x2c9c6f0..0x2c9c728: built-in views in the active mask below the count.
inline uint32_t builtinActive(uint64_t mask, int32_t count) {
    uint32_t n{};
    for (int32_t i = 0; i < count && i < 64; ++i)
        n += (mask >> i) & 1 ? 1u : 0u;
    return n;
}

enum Kind : uint32_t { gameView, eyeLeft, eyeRight, offscreen, other, kinds };
inline constexpr const char* kindNames[kinds] = {"game", "eye_left", "eye_right", "offscreen", "other"};
// A view (or a render copy of one: its owner) as the probe sees it.
inline Kind classify(uint64_t view, uint64_t owner, uint64_t primary, uint64_t left, uint64_t right,
                     const uint64_t* slotViews) {
    if (!view)
        return other;
    if (left && (view == left || owner == left))
        return eyeLeft;
    if (right && (view == right || owner == right))
        return eyeRight;
    if (primary && (view == primary || owner == primary))
        return gameView;
    for (uint32_t i = 0; i < slots; ++i)
        if (slotViews[i] && (view == slotViews[i] || owner == slotViews[i]))
            return offscreen;
    return other;
}
// stereo_probe's sameRoot: a piece of the character shares his root transform (3 cm, 0.998).
inline bool sameRoot(const float* m, const float* hero) {
    const float dx = m[12] - hero[12], dy = m[13] - hero[13], dz = m[14] - hero[14];
    if (!(dx * dx + dy * dy + dz * dz <= .03f * .03f))
        return false;
    for (int row = 0; row < 3; ++row)
        if (!(m[row * 4] * hero[row * 4] + m[row * 4 + 1] * hero[row * 4 + 1] + m[row * 4 + 2] * hero[row * 4 + 2] >=
              .998f))
            return false;
    return true;
}

// How often: draws are looked at in one frame of `sampleEvery`; the suit fields every `snapEvery` frames.
inline constexpr uint32_t sampleEvery = 32, snapEvery = 36, publishEvery = 8;
inline constexpr double budgetUs = 60; // mean per watched frame, over `budgetFrames`; above: it stops
inline constexpr uint32_t budgetFrames = 600;
// At most this many draws are looked at in a sampled frame (two 64-byte reads each); the rest return at once.
inline constexpr uint32_t drawCap = 4096;
enum Status : uint32_t { notStarted, watching, stoppedSlow, stoppedFault };
inline constexpr const char* statusNames[] = {"not started", "watching", "stopped: too slow", "stopped: fault"};

// The suit snapshot: the hero's instance, his model and his actor record, read-only copies.
inline constexpr uint32_t instanceBytes = 0x100, modelBytes = 0x100, recordBytes = 0x400;
inline constexpr uint32_t snapWords = (instanceBytes + modelBytes + recordBytes) / 4;
enum Where : uint16_t { inInstance, inModel, inRecord };
inline void whereOf(uint32_t word, uint16_t& where, uint16_t& offset) {
    const uint32_t byte = word * 4;
    if (byte < instanceBytes) {
        where = inInstance;
        offset = static_cast<uint16_t>(byte);
    } else if (byte < instanceBytes + modelBytes) {
        where = inModel;
        offset = static_cast<uint16_t>(byte - instanceBytes);
    } else {
        where = inRecord;
        offset = static_cast<uint16_t>(byte - instanceBytes - modelBytes);
    }
}

#pragma pack(push, 4)
struct SlotEvent { // a slot of the 8 became another view (or empty)
    uint32_t frame{}, slot{}, created{}, ours{}, flags{}, width{}, height{}, kind{};
};
struct Piece { // an instance drawn on the hero's root transform (not the hero instance itself)
    uint64_t instance{}, model{};
    float radius{};
    uint32_t flags{};
    uint32_t draws[kinds]{};
    uint32_t pad{};
};
struct Field { // a 4-byte field that changed with the hero's model (a suit-ID candidate)
    uint16_t where{}, offset{}, withSuit{}, noise{};
    uint32_t before{}, after{};
};
struct SuitEvent { // the hero's instance or model became another
    uint32_t frame{}, what{}; // what: 1 instance, 2 model, 3 both
    uint64_t oldModel{}, newModel{};
    uint32_t changedWords{}, pad{};
};
#pragma pack(pop)
static_assert(sizeof(SlotEvent) == 32 && sizeof(Piece) == 48 && sizeof(Field) == 16 && sizeof(SuitEvent) == 32);

inline constexpr uint32_t eventCount = 8, pieceCount = 8, fieldCount = 16, suitEventCount = 6;
inline constexpr uint32_t firstEventCount = 16, previewCount = 8, previewSnapEvery = 9;
#pragma pack(push, 4)
struct PreviewInstance { // an instance drawn in an offscreen slot that is not the port's (Update 38.5)
    uint64_t instance{}, model{};
    uint32_t draws{}, slot{};
    uint32_t hero{};      // 1: the hero's own instance; 2: on the hero's root; 3: the hero's model
    uint32_t flags{};
};
struct V2 {
    uint32_t eventMs[eventCount]{}, suitMs[suitEventCount]{}; // ms since the probe started
    SlotEvent first[firstEventCount]{};
    uint32_t firstMs[firstEventCount]{};
    uint32_t firstCount{}, startSlotsUsed{}; // the slots in use at the probe's first frame
    uint64_t heroDrawsBySlot[slots]{}, otherDrawsBySlot[slots]{};
    uint64_t menuFrames{}, previewFrames{}, previewStarts{};
    uint32_t previewSlot = 0xffffffffu, previewWidth{}, previewHeight{}, previewFlags{}, previewKind{};
    uint32_t previewFirstMs{}, previewLastMs{}, previewInstances{};
    uint64_t solvedInPreview{}, heroJobsInPreview{}, solvedOutside{}, bodyDrawnInPreview{};
    uint32_t rigSwitchesInPreview{}, modelChangesInPreview{}, instanceChangesInPreview{}, snapsInPreview{};
    PreviewInstance preview[previewCount]{};
};
#pragma pack(pop)
static_assert(sizeof(PreviewInstance) == 32);
// telemetry: SpidyMirrorProbeData (tools/update37_records.py 'mirror_probe')
inline constexpr uint32_t dataMagic = 0x4252504d; // MPRB
struct Data {
    uint32_t magic = dataMagic, version = 2, bytes = 2752, status{};
    uint64_t frames{};
    // the 8 offscreen slots and the view list
    uint32_t slotsUsedNow{}, slotsUsedMax{}, slotsOursNow{}, slotsFreeMin = slots;
    uint32_t builtinNow{}, builtinMax{}, listedNow{}, listedMax{};
    int32_t headroomMin = static_cast<int32_t>(smallestCap); // 8 - (built-in + listed offscreen), the least seen
    uint32_t eventNext{};
    uint64_t slotCreated{}, slotRemoved{}, slotOursCreated{};
    SlotEvent events[eventCount]{};
    uint32_t slotFlags[slots]{}, slotWidth[slots]{}, slotHeight[slots]{}, slotKind[slots]{};
    // render jobs per view kind (the port's copyJob hook)
    uint64_t jobs[kinds]{};
    uint32_t jobsFrameMax{}, viewsFrameMax{};
    // the hero's draws, in sampled frames (the port's renderActor hook, before its own hiding)
    uint64_t sampledFrames{}, sampledGameFrames{}, heroMissingInGame{};
    uint64_t heroDraws[kinds]{}, pieceDraws[kinds]{};
    uint32_t pieces{}, piecesOverflow{};
    Piece piece[pieceCount]{};
    // hiding
    uint64_t heroFrames{}, hiddenFrames{}, hiddenByPort{}, hiddenByGame{}, gameHides{};
    uint64_t portHideFrames{}, bodyDrawnFrames{};
    uint32_t heroFlagsNow{}, cameraSwitchNow{}, cameraSwitchChanges{}, cappedFrames{}; // cappedFrames: drawCap reached
    // the suit
    uint64_t heroInstance{}, heroModel{}, heroRecord{};
    uint64_t instanceChanges{}, modelChanges{}, snaps{};
    uint32_t suitNext{}, fields{};
    SuitEvent suit[suitEventCount]{};
    Field field[fieldCount]{};
    // cost (main thread, per watched frame)
    double costUsTotal{};
    float costUsMax{}, costUsMean{};
    uint64_t sampledDraws{};
    V2 v2{}; // Update 38.5 PROBE v2
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 2752 && offsetof(Data, sampledDraws) == 1608, "Data.bytes and update37_records.RECORDS['mirror_probe']");

// The suit tally: which words change when the model does, and which change anyway.
struct Tally {
    uint32_t last[snapWords]{}, now[snapWords]{};
    uint16_t withSuit[snapWords]{}, noise[snapWords]{};
    bool have{};
    // One snapshot; `changed`: the hero's instance or model changed since the last one. Returns the
    // number of words that differ from the last snapshot (0 for the first).
    uint32_t step(const uint32_t* words, bool changed) {
        uint32_t differ{};
        std::memcpy(now, words, sizeof now);
        if (have)
            for (uint32_t i = 0; i < snapWords; ++i)
                if (now[i] != last[i]) {
                    ++differ;
                    auto& c = changed ? withSuit[i] : noise[i];
                    if (c < 0xffff)
                        ++c;
                }
        if (changed && have)
            for (uint32_t i = 0; i < snapWords; ++i)
                if (now[i] != last[i])
                    before[i] = last[i];
        std::memcpy(last, now, sizeof last);
        have = true;
        return differ;
    }
    uint32_t before[snapWords]{};
    // The best candidates: changed with the model more often than at other times.
    uint32_t top(Field* out, uint32_t n) const {
        uint32_t used{};
        for (uint32_t i = 0; i < snapWords; ++i) {
            if (!withSuit[i] || withSuit[i] <= noise[i])
                continue;
            const int score = withSuit[i] * 4 - noise[i];
            uint32_t at = used;
            while (at > 0 && score > out[at - 1].withSuit * 4 - out[at - 1].noise)
                --at;
            if (at >= n)
                continue;
            for (uint32_t k = std::min(used, n - 1); k > at; --k)
                out[k] = out[k - 1];
            Field f;
            whereOf(i, f.where, f.offset);
            f.withSuit = withSuit[i];
            f.noise = noise[i];
            f.before = before[i];
            f.after = last[i];
            out[at] = f;
            used = std::min(used + 1, n);
        }
        return used;
    }
};

// The probe's own cost; false once the mean over `budgetFrames` frames is above the budget.
struct Cost {
    double total{};
    float max{};
    uint64_t frames{};
    bool add(double us) {
        total += us;
        max = std::max(max, static_cast<float>(us));
        ++frames;
        return frames < budgetFrames || total / static_cast<double>(frames) <= budgetUs;
    }
    float mean() const { return frames ? static_cast<float>(total / static_cast<double>(frames)) : 0.f; }
};
} // namespace spidy::mirror_probe

// mirror_probe.cpp (Windows): fed by stereo_probe.cpp's existing hooks. Read-only.
namespace spidy::mirror_probe {
struct Frame { // the main thread, once a frame after the game's view maintenance
    uintptr_t base{};
    uint64_t primary{}, left{}, right{};   // the game's view, the eyes
    uint64_t record{};                     // the hero's actor record (its first field: his instance)
    uint64_t portHidden{};                 // the instance the port switched off (DrawOff), or 0
    bool portHides{}, bodyDrawn{};
    // Update 38.5 PROBE v2: the port's body (native_body::status()): solves, hero pose jobs, rig switches
    uint64_t bodySolved{}, bodyHeroJobs{};
    uint32_t bodyRigSwitches{};
};
void start(uintptr_t base);
void frame(const Frame&);
void job(uint64_t view);                              // a render job copied `view` (any thread)
bool sampling();                                      // draws are looked at this frame
void draw(uint64_t view, uint64_t owner, uint64_t instance); // an actor drawn in `view` (sampled frames)
// Version 0.0.4: sampled draws in one offscreen slot: the hero (his instance or a piece on his root)
// and everything else (the MIRROR VIEW TEST's answer).
void slotDraws(uint32_t slot, uint64_t& hero, uint64_t& other);
} // namespace spidy::mirror_probe
