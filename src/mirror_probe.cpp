// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// Update 38.4: the MIRROR PROBE (include/spidy/mirror_probe.hpp, docs/mirror.md). READ-ONLY: every
// read of the game goes through peek() (SEH); nothing here writes game memory, calls a game function,
// changes page protection or makes a hook. The port's own hooks (stereo_probe.cpp) hand it values.
#include <windows.h>

#include <atomic>
#include <cstring>

#include "spidy/mirror_probe.hpp"

extern "C" {
__declspec(dllexport) spidy::mirror_probe::Data SpidyMirrorProbeData;
}

namespace spidy::mirror_probe {
namespace {
std::atomic<uint32_t> status{notStarted};
std::atomic<bool> sampled{};
std::atomic<uint64_t> heroAt{}, primaryAt{}, leftAt{}, rightAt{};
std::atomic<uint64_t> slotAt[slots]{};
std::atomic<uint32_t> frameJobs{}, frameDraws{}, frameViews{}; // frameViews: a bit per view (kind, or slot)
std::atomic<bool> gameJob{}; // the game's view had a render job since the last frame
std::atomic<uint64_t> jobs[kinds]{}, heroDraws[kinds]{}, pieceDraws[kinds]{}, sampledDraws{};
std::atomic<uint64_t> gameDrawsOfHero{}; // this sampled frame
SRWLOCK pieceLock = SRWLOCK_INIT;
Piece pieces[pieceCount]{}; // pieceLock
uint32_t piecesUsed{}, piecesOverflow{}; // pieceLock
uintptr_t imageBase{};
Data work;   // main thread
Tally tally; // main thread
Cost cost;   // main thread
uint64_t frameNo{}, lastSlot[slots]{}, lastInstance{}, lastModel{};
bool changedSinceSnap{}, lastHidden{};
uint32_t lastSwitch = 0xffffffffu;
uint32_t snapWordsNow[snapWords]{};
double tickUs{};
// Update 38.5 PROBE v2
LARGE_INTEGER startedAt{};
std::atomic<uint64_t> heroBySlot[slots]{}, otherBySlot[slots]{}, pieceBySlot[slots]{};
std::atomic<uint32_t> previewSlotNow{0xffffffffu}; // a slot that drew anything this sampled frame
PreviewInstance previews[previewCount]{}; // pieceLock
uint32_t previewsUsed{};                   // pieceLock
bool previewWas{}, firstFrame = true;
uint64_t lastSolved{}, lastHeroJobs{};
uint32_t lastRigSwitches{};
uint32_t msNow() {
    LARGE_INTEGER t{};
    QueryPerformanceCounter(&t);
    return static_cast<uint32_t>(static_cast<double>(t.QuadPart - startedAt.QuadPart) * tickUs / 1000.0);
}

bool peek(uint64_t p, void* out, size_t n) {
    __try {
        if (p < 0x10000)
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
uint32_t peek32(uint64_t p) {
    uint32_t v{};
    return peek(p, &v, 4) ? v : 0;
}
void publish() {
    auto& d = SpidyMirrorProbeData;
    const uint64_t s = d.sequence;
    reinterpret_cast<std::atomic<uint64_t>&>(d.sequence).store(s + 1, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_release);
    std::memcpy(static_cast<void*>(&d), &work, offsetof(Data, sequence));
    std::atomic_thread_fence(std::memory_order_release);
    reinterpret_cast<std::atomic<uint64_t>&>(d.sequence).store(s + 2, std::memory_order_release);
}
void stopAs(uint32_t why) {
    sampled = false;
    status = why;
    work.status = why;
    publish();
}
void slotsStep(uint64_t manager) {
    uint32_t used{}, ours{}, listedOffscreen{};
    const uint64_t left = leftAt.load(std::memory_order_relaxed), right = rightAt.load(std::memory_order_relaxed);
    for (uint32_t i = 0; i < slots; ++i) {
        const uint64_t view = peek64(manager + slotsAt + i * 8ull);
        slotAt[i].store(view, std::memory_order_relaxed);
        uint32_t flags{}, width{}, height{}, kind{};
        if (view) {
            ++used;
            const bool own = view == left || view == right;
            ours += own ? 1u : 0u;
            flags = peek32(view + flagsAt);
            width = peek32(view + widthAt);
            height = peek32(view + heightAt);
            kind = peek32(view + kindAt);
            listedOffscreen += listed(flags) ? 1u : 0u;
        }
        work.slotFlags[i] = flags;
        work.slotWidth[i] = width;
        work.slotHeight[i] = height;
        work.slotKind[i] = kind;
        if (view != lastSlot[i]) {
            const bool own = view && (view == left || view == right);
            if (view) {
                ++work.slotCreated;
                work.slotOursCreated += own ? 1u : 0u;
            } else {
                ++work.slotRemoved;
            }
            auto& e = work.events[work.eventNext % eventCount];
            e = SlotEvent{static_cast<uint32_t>(frameNo), i, view ? 1u : 0u, own ? 1u : 0u, flags, width, height, kind};
            const uint32_t ms = msNow();
            work.v2.eventMs[work.eventNext % eventCount] = ms;
            ++work.eventNext;
            if (work.v2.firstCount < firstEventCount) { // kept for good: made at start, or mid-game?
                work.v2.first[work.v2.firstCount] = e;
                work.v2.firstMs[work.v2.firstCount] = ms;
                ++work.v2.firstCount;
            }
            lastSlot[i] = view;
        }
    }
    uint64_t mask{};
    int32_t count{};
    peek(manager + maskAt, &mask, 8);
    peek(manager + countAt, &count, 4);
    const uint32_t builtin = builtinActive(mask, count);
    if (firstFrame) {
        work.v2.startSlotsUsed = used;
        firstFrame = false;
    }
    work.slotsUsedNow = used;
    work.slotsUsedMax = std::max(work.slotsUsedMax, used);
    work.slotsOursNow = ours;
    work.slotsFreeMin = std::min(work.slotsFreeMin, slots - used);
    work.builtinNow = builtin;
    work.builtinMax = std::max(work.builtinMax, builtin);
    work.listedNow = builtin + listedOffscreen;
    work.listedMax = std::max(work.listedMax, work.listedNow);
    work.headroomMin = std::min(work.headroomMin, static_cast<int32_t>(smallestCap) - static_cast<int32_t>(work.listedNow));
}
void heroStep(const Frame& f) {
    const uint64_t instance = f.record ? peek64(f.record) : 0;
    heroAt.store(instance, std::memory_order_relaxed);
    work.heroRecord = f.record;
    work.heroInstance = instance;
    if (f.portHides)
        ++work.portHideFrames;
    if (f.bodyDrawn)
        ++work.bodyDrawnFrames;
    if (!instance)
        return;
    ++work.heroFrames;
    const uint32_t flags = peek32(instance + instanceFlagsAt);
    const uint64_t model = peek64(instance + modelAt);
    work.heroFlagsNow = flags;
    work.heroModel = model;
    const bool hidden = (flags & drawnOff) != 0;
    const bool byPort = hidden && f.portHidden == instance;
    if (hidden) {
        ++work.hiddenFrames;
        if (byPort)
            ++work.hiddenByPort;
        else
            ++work.hiddenByGame;
    }
    if (hidden && !byPort && !lastHidden)
        ++work.gameHides;
    lastHidden = hidden && !byPort;
    uint32_t what{};
    if (lastInstance && instance != lastInstance)
        what |= 1;
    if (lastModel && model != lastModel)
        what |= 2;
    if (what) {
        work.instanceChanges += what & 1 ? 1u : 0u;
        work.modelChanges += what & 2 ? 1u : 0u;
        auto& e = work.suit[work.suitNext % suitEventCount];
        e = SuitEvent{static_cast<uint32_t>(frameNo), what, lastModel, model, 0, 0};
        work.v2.suitMs[work.suitNext % suitEventCount] = msNow();
        ++work.suitNext;
        if (previewWas) {
            work.v2.modelChangesInPreview += what & 2 ? 1u : 0u;
            work.v2.instanceChangesInPreview += what & 1 ? 1u : 0u;
        }
        changedSinceSnap = true;
    }
    lastInstance = instance;
    lastModel = model;
    // Update 38.5: 4x as often while a preview is up (the suits menu changes the suit there).
    if (frameNo % (previewWas ? previewSnapEvery : snapEvery))
        return;
    work.v2.snapsInPreview += previewWas ? 1u : 0u;
    // The suit snapshot: unread parts stay 0 (a failed read is not a change).
    std::memset(snapWordsNow, 0, sizeof snapWordsNow);
    uint32_t* const parts[3] = {snapWordsNow, snapWordsNow + instanceBytes / 4,
                                snapWordsNow + (instanceBytes + modelBytes) / 4};
    if (!peek(instance, parts[0], instanceBytes))
        std::memset(parts[0], 0, instanceBytes);
    if (!model || !peek(model, parts[1], modelBytes))
        std::memset(parts[1], 0, modelBytes);
    if (!peek(f.record, parts[2], recordBytes))
        std::memset(parts[2], 0, recordBytes);
    const uint32_t differ = tally.step(snapWordsNow, changedSinceSnap);
    if (changedSinceSnap && work.suitNext)
        work.suit[(work.suitNext - 1) % suitEventCount].changedWords = differ;
    changedSinceSnap = false;
    ++work.snaps;
    for (auto& old : work.field)
        old = Field{};
    work.fields = tally.top(work.field, fieldCount);
}
} // namespace

void start(uintptr_t base) {
    imageBase = base;
    LARGE_INTEGER q{};
    QueryPerformanceFrequency(&q);
    tickUs = q.QuadPart ? 1e6 / static_cast<double>(q.QuadPart) : 0;
    work = Data{};
    QueryPerformanceCounter(&startedAt);
    work.status = watching;
    status = watching;
    publish();
}

namespace {
// One frame's work. Every game read in it goes through peek(); guarded() is the belt and braces.
void frameWork(const Frame& f) {
    ++frameNo;
    ++work.frames;
    primaryAt.store(f.primary, std::memory_order_relaxed);
    leftAt.store(f.left, std::memory_order_relaxed);
    rightAt.store(f.right, std::memory_order_relaxed);
    slotsStep(imageBase + managerRva);
    heroStep(f);
    // the sampled frame that just ended
    if (sampled.load(std::memory_order_relaxed)) {
        ++work.sampledFrames;
        if (gameJob.load(std::memory_order_relaxed)) {
            // the game's view rendered in it (a job): was the hero drawn there?
            ++work.sampledGameFrames;
            if (!gameDrawsOfHero.load(std::memory_order_relaxed))
                ++work.heroMissingInGame;
        }
    }
    // Update 38.5 PROBE v2: the sampled frame that just ended - a preview up (an offscreen slot
    // that is not the port's drew something), the game's view not rendered (a menu: CANDIDATE),
    // the port's body solves while the preview is up.
    if (sampled.load(std::memory_order_relaxed)) {
        const uint32_t slot = previewSlotNow.exchange(0xffffffffu, std::memory_order_relaxed);
        const bool preview = slot < slots;
        if (!gameJob.load(std::memory_order_relaxed))
            ++work.v2.menuFrames;
        if (preview) {
            ++work.v2.previewFrames;
            work.v2.previewStarts += previewWas ? 0u : 1u;
            work.v2.previewSlot = slot;
            work.v2.previewWidth = work.slotWidth[slot];
            work.v2.previewHeight = work.slotHeight[slot];
            work.v2.previewFlags = work.slotFlags[slot];
            work.v2.previewKind = work.slotKind[slot];
            const uint32_t ms = msNow();
            if (!work.v2.previewFirstMs)
                work.v2.previewFirstMs = ms ? ms : 1;
            work.v2.previewLastMs = ms;
        }
        previewWas = preview;
    }
    if (lastSolved || lastHeroJobs) {
        const uint64_t solved = f.bodySolved >= lastSolved ? f.bodySolved - lastSolved : 0;
        const uint64_t heroJobs = f.bodyHeroJobs >= lastHeroJobs ? f.bodyHeroJobs - lastHeroJobs : 0;
        if (previewWas) {
            work.v2.solvedInPreview += solved;
            work.v2.heroJobsInPreview += heroJobs;
            work.v2.bodyDrawnInPreview += f.bodyDrawn ? 1u : 0u;
            work.v2.rigSwitchesInPreview += f.bodyRigSwitches > lastRigSwitches ? f.bodyRigSwitches - lastRigSwitches : 0u;
        } else {
            work.v2.solvedOutside += solved;
        }
    }
    lastSolved = f.bodySolved;
    lastHeroJobs = f.bodyHeroJobs;
    lastRigSwitches = f.bodyRigSwitches;
    if (frameDraws.exchange(0, std::memory_order_relaxed) > drawCap)
        ++work.cappedFrames;
    const uint32_t frameJobsNow = frameJobs.exchange(0, std::memory_order_relaxed);
    work.jobsFrameMax = std::max(work.jobsFrameMax, frameJobsNow);
    const uint32_t views = frameViews.exchange(0, std::memory_order_relaxed);
    uint32_t distinct{};
    for (uint32_t b = 0; b < 32; ++b)
        distinct += (views >> b) & 1;
    work.viewsFrameMax = std::max(work.viewsFrameMax, distinct);
    gameJob = false;
    gameDrawsOfHero = 0;
    sampled.store(frameNo % sampleEvery == 0, std::memory_order_relaxed);
    const uint32_t sw = peek32(imageBase + cameraSwitchRva) & 0xff;
    if (lastSwitch != 0xffffffffu && sw != lastSwitch)
        ++work.cameraSwitchChanges;
    lastSwitch = sw;
    work.cameraSwitchNow = sw;
    if (frameNo % publishEvery == 0) {
        for (uint32_t k = 0; k < kinds; ++k) {
            work.jobs[k] = jobs[k].load(std::memory_order_relaxed);
            work.heroDraws[k] = heroDraws[k].load(std::memory_order_relaxed);
            work.pieceDraws[k] = pieceDraws[k].load(std::memory_order_relaxed);
        }
        work.sampledDraws = sampledDraws.load(std::memory_order_relaxed);
        for (uint32_t i = 0; i < slots; ++i) {
            work.v2.heroDrawsBySlot[i] = heroBySlot[i].load(std::memory_order_relaxed);
            work.v2.otherDrawsBySlot[i] = otherBySlot[i].load(std::memory_order_relaxed);
        }
        AcquireSRWLockShared(&pieceLock);
        std::memcpy(work.piece, pieces, sizeof pieces);
        work.pieces = piecesUsed;
        work.piecesOverflow = piecesOverflow;
        std::memcpy(work.v2.preview, previews, sizeof previews);
        work.v2.previewInstances = previewsUsed;
        ReleaseSRWLockShared(&pieceLock);
    }
}
bool guarded(const Frame& f) {
    __try {
        frameWork(f);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
} // namespace

void frame(const Frame& f) {
    if (status.load(std::memory_order_relaxed) != watching || !imageBase)
        return;
    LARGE_INTEGER t0{}, t1{};
    QueryPerformanceCounter(&t0);
    if (!guarded(f)) {
        stopAs(stoppedFault);
        return;
    }
    QueryPerformanceCounter(&t1);
    const bool cheap = cost.add(static_cast<double>(t1.QuadPart - t0.QuadPart) * tickUs);
    work.costUsTotal = cost.total;
    work.costUsMax = cost.max;
    work.costUsMean = cost.mean();
    if (!cheap) {
        stopAs(stoppedSlow);
        return;
    }
    if (frameNo % publishEvery == 0)
        publish();
}

void job(uint64_t view) {
    if (status.load(std::memory_order_relaxed) != watching)
        return;
    uint64_t slotViews[slots];
    for (uint32_t i = 0; i < slots; ++i)
        slotViews[i] = slotAt[i].load(std::memory_order_relaxed);
    const Kind k = classify(view, 0, primaryAt.load(std::memory_order_relaxed), leftAt.load(std::memory_order_relaxed),
                            rightAt.load(std::memory_order_relaxed), slotViews);
    jobs[k].fetch_add(1, std::memory_order_relaxed);
    frameJobs.fetch_add(1, std::memory_order_relaxed);
    uint32_t bit = 1u << k;
    if (k == offscreen)
        for (uint32_t i = 0; i < slots; ++i)
            if (slotViews[i] == view)
                bit = 1u << (8 + i);
    frameViews.fetch_or(bit, std::memory_order_relaxed);
    if (k == gameView)
        gameJob.store(true, std::memory_order_relaxed);
}

bool sampling() { return sampled.load(std::memory_order_relaxed); }

void slotDraws(uint32_t slot, uint64_t& hero, uint64_t& other) {
    hero = other = 0;
    if (slot >= slots)
        return;
    const uint64_t onRoot = pieceBySlot[slot].load(std::memory_order_relaxed);
    const uint64_t others = otherBySlot[slot].load(std::memory_order_relaxed);
    hero = heroBySlot[slot].load(std::memory_order_relaxed) + onRoot;
    other = others > onRoot ? others - onRoot : 0;
}

void draw(uint64_t view, uint64_t owner, uint64_t instance) {
    const uint64_t hero = heroAt.load(std::memory_order_relaxed);
    if (!hero || !instance || status.load(std::memory_order_relaxed) != watching)
        return;
    if (frameDraws.fetch_add(1, std::memory_order_relaxed) >= drawCap)
        return; // the cost bound: a crowded sampled frame looks at its first drawCap draws only
    sampledDraws.fetch_add(1, std::memory_order_relaxed);
    uint64_t slotViews[slots];
    for (uint32_t i = 0; i < slots; ++i)
        slotViews[i] = slotAt[i].load(std::memory_order_relaxed);
    const Kind k = classify(view, owner, primaryAt.load(std::memory_order_relaxed),
                            leftAt.load(std::memory_order_relaxed), rightAt.load(std::memory_order_relaxed), slotViews);
    bool piece{}, onRoot{};
    if (instance != hero) {
        float m[16]{}, h[16]{};
        onRoot = peek(instance, m, sizeof m) && peek(hero, h, sizeof h) && sameRoot(m, h);
        piece = onRoot;
    }
    // Update 38.5 PROBE v2: anything drawn in an offscreen slot (the suits menu's preview?),
    // by slot, and its first instances: the hero's own, on his root, his model, or another.
    if (k == offscreen) {
        uint32_t slot = slots;
        for (uint32_t i = 0; i < slots; ++i)
            if (slotViews[i] && (view == slotViews[i] || owner == slotViews[i]))
                slot = i;
        if (slot < slots) {
            (instance == hero ? heroBySlot : otherBySlot)[slot].fetch_add(1, std::memory_order_relaxed);
            if (onRoot)
                pieceBySlot[slot].fetch_add(1, std::memory_order_relaxed);
            previewSlotNow.store(slot, std::memory_order_relaxed);
            const uint64_t model = peek64(instance + modelAt);
            const uint32_t heroIs = instance == hero                   ? 1u
                                    : onRoot                           ? 2u
                                    : model && model == peek64(hero + modelAt) ? 3u
                                                                       : 0u;
            const uint32_t flags = peek32(instance + instanceFlagsAt);
            AcquireSRWLockExclusive(&pieceLock);
            uint32_t at = 0;
            while (at < previewsUsed && previews[at].instance != instance)
                ++at;
            if (at == previewsUsed && previewsUsed < previewCount)
                previews[previewsUsed++] = PreviewInstance{instance, model, 0, slot, heroIs, flags};
            if (at < previewsUsed) {
                ++previews[at].draws;
                previews[at].model = model;
                previews[at].hero = heroIs;
                previews[at].flags = flags;
            }
            ReleaseSRWLockExclusive(&pieceLock);
        }
    }
    if (instance != hero && !piece)
        return;
    if (!piece) {
        heroDraws[k].fetch_add(1, std::memory_order_relaxed);
        if (k == gameView)
            gameDrawsOfHero.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    pieceDraws[k].fetch_add(1, std::memory_order_relaxed);
    const uint64_t model = peek64(instance + modelAt);
    float radius{};
    peek(instance + radiusAt, &radius, 4);
    const uint32_t flags = peek32(instance + instanceFlagsAt);
    AcquireSRWLockExclusive(&pieceLock);
    uint32_t at = 0;
    while (at < piecesUsed && pieces[at].instance != instance)
        ++at;
    if (at == piecesUsed) {
        if (piecesUsed < pieceCount)
            pieces[piecesUsed++] = Piece{instance, model, radius, flags, {}, 0};
        else
            ++piecesOverflow;
    }
    if (at < piecesUsed) {
        pieces[at].model = model;
        pieces[at].radius = radius;
        pieces[at].flags = flags;
        ++pieces[at].draws[k];
    }
    ReleaseSRWLockExclusive(&pieceLock);
}
} // namespace spidy::mirror_probe
