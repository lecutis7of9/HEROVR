#pragma once
// 0.0.74 test 2b TAA DIAGNOSTICS (owner, relayed: "that TAA setting still shakes so bad ... I can't get a clear
// nice image"; no guessed preset). OFF by default. While ON and ENGINE TAA is on, every frame records, per eye,
// what TemporalAa reads from the eye's view (read() only, engine_taa offsets): the jitter we SENT to the lens
// vs the jitter the view STORED (+0x428), the history cut/stamp ages and the current-frame weight they give
// (1 = history thrown away), and the eye's buffer set (+0x18e0: two different sets = per-eye history, one =
// shared). The previous view-projection is NOT read: no CONFIRMED offset for it in this exe (a CANDIDATE gap).
// Shared with tools/run_game_vr.py (SpidyTaaDiag, TAA_DIAG_FORMAT).
#include <cmath>
#include <cstdint>

namespace spidy::taa_diag {
inline constexpr uint32_t magic = 0x44415454; // TTAD
inline constexpr uint32_t version = 1;
inline constexpr uint32_t ringSize = 16;
struct Frame {
    uint32_t frame{};
    float sent[2]{};       // the lens jitter we passed (both eyes get the same, as SpidyVR)
    float stored[4]{};     // left x, y, right x, y: +0x428/+0x42c after the lens
    int32_t cutAge[2]{}, stampAge[2]{};
    float weight[2]{};
};
static_assert(sizeof(Frame) == 52);
struct Diag {
    uint32_t magic = taa_diag::magic, version = taa_diag::version, bytes{}, on{};
    uint64_t frames{};                 // frames recorded
    uint32_t resets[2]{};              // frames with weight 1 (history thrown away), per eye
    uint32_t stampChanges[2]{};        // frames whose history stamp moved since the previous frame
    uint32_t sharedJitterFrames{};     // frames the two eyes stored the same jitter
    uint32_t sharedSetFrames{};        // frames both eyes had the same buffer set (shared history)
    uint32_t perEyeSetFrames{};        // frames each eye had its own buffer set
    uint32_t sentMismatchFrames{};     // frames stored != sent x 1 and != sent x 2 (beyond 1e-6)
    float storedOverSent[2]{};         // the newest stored / sent (x, y), 0 when sent is 0
    uint32_t head{}, reserved{};
    Frame ring[ringSize]{};
    uint64_t sequence{};
};
static_assert(sizeof(Diag) == 912);
// stored / sent, 0 where sent is ~0 or anything is not finite.
inline float ratio(float stored, float sent) {
    if (!std::isfinite(stored) || !std::isfinite(sent) || std::abs(sent) < 1e-7f)
        return 0;
    return stored / sent;
}
// The stored jitter is the sent one taken as-is (x1) or doubled by the lens (x2): both CONFIRMED patterns.
inline bool storedMatches(float stored, float sent) {
    return std::abs(stored - sent) <= 1e-6f || std::abs(stored - 2 * sent) <= 1e-6f;
}
// One frame into the record. setL/setR: the eyes' buffer sets (0 when unread). lastStamp: per eye, kept by caller.
inline void add(Diag& d, const Frame& f, uintptr_t setL, uintptr_t setR, int32_t stampL, int32_t stampR,
                int32_t lastStamp[2], bool& haveLast) {
    d.ring[d.head % ringSize] = f;
    d.head = (d.head + 1) % ringSize;
    ++d.frames;
    for (unsigned i = 0; i < 2; ++i)
        if (f.weight[i] >= 1)
            ++d.resets[i];
    const int32_t stamps[2]{stampL, stampR};
    if (haveLast)
        for (unsigned i = 0; i < 2; ++i)
            if (stamps[i] != lastStamp[i])
                ++d.stampChanges[i];
    lastStamp[0] = stampL;
    lastStamp[1] = stampR;
    haveLast = true;
    if (f.stored[0] == f.stored[2] && f.stored[1] == f.stored[3])
        ++d.sharedJitterFrames;
    if (setL && setR)
        ++(setL == setR ? d.sharedSetFrames : d.perEyeSetFrames);
    if (!storedMatches(f.stored[0], f.sent[0]) || !storedMatches(f.stored[1], f.sent[1]))
        ++d.sentMismatchFrames;
    d.storedOverSent[0] = ratio(f.stored[0], f.sent[0]);
    d.storedOverSent[1] = ratio(f.stored[1], f.sent[1]);
    ++d.sequence;
}
} // namespace spidy::taa_diag
