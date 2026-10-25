// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// Update 38.4: the MIRROR PROBE's logic (include/spidy/mirror_probe.hpp): the view list rule as the
// exe has it, the view kinds, the suit tally, the cost guard, and that a frame's work is cheap.
// `--dump <file>` writes a filled record for tests/update384_tests.py (the Python parser).
#include "spidy/mirror_probe.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>

using namespace spidy::mirror_probe;
namespace {
int failures{};
void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "  PASS" : "  FAIL", what);
    failures += ok ? 0 : 1;
}
int dump(const char* path) {
    Data d;
    d.status = watching;
    d.frames = 1234;
    d.slotsUsedNow = 3;
    d.slotsUsedMax = 4;
    d.slotsOursNow = 2;
    d.slotsFreeMin = 4;
    d.builtinNow = 2;
    d.builtinMax = 3;
    d.listedNow = 5;
    d.listedMax = 6;
    d.headroomMin = 2;
    d.slotCreated = 5;
    d.slotRemoved = 1;
    d.slotOursCreated = 2;
    d.events[0] = SlotEvent{10, 2, 1, 0, 0x40, 512, 512, 7};
    d.eventNext = 1;
    d.slotFlags[2] = 0x40;
    d.slotWidth[2] = 512;
    d.slotHeight[2] = 512;
    d.slotKind[2] = 7;
    d.jobs[gameView] = 100;
    d.jobs[eyeLeft] = 101;
    d.jobs[eyeRight] = 102;
    d.jobs[offscreen] = 7;
    d.jobsFrameMax = 9;
    d.viewsFrameMax = 4;
    d.sampledFrames = 38;
    d.sampledGameFrames = 30;
    d.heroMissingInGame = 3;
    d.heroDraws[eyeLeft] = 11;
    d.pieceDraws[eyeRight] = 12;
    d.pieces = 1;
    d.piece[0] = Piece{0x1234, 0x5678, 0.25f, 0x20, {1, 2, 3, 4, 5}, 0};
    d.heroFrames = 1200;
    d.hiddenFrames = 900;
    d.hiddenByPort = 880;
    d.hiddenByGame = 20;
    d.gameHides = 2;
    d.portHideFrames = 890;
    d.bodyDrawnFrames = 10;
    d.heroFlagsNow = 0x21;
    // Update 38.5 PROBE v2
    d.v2.eventMs[0] = 4321;
    d.v2.first[0] = SlotEvent{1, 0, 1, 0, 0x8, 1920, 1080, 1};
    d.v2.firstMs[0] = 16;
    d.v2.first[1] = d.events[0];
    d.v2.firstMs[1] = 4321;
    d.v2.firstCount = 2;
    d.v2.startSlotsUsed = 2;
    d.v2.heroDrawsBySlot[2] = 40;
    d.v2.otherDrawsBySlot[2] = 7;
    d.v2.menuFrames = 15;
    d.v2.previewFrames = 12;
    d.v2.previewStarts = 3;
    d.v2.previewSlot = 2;
    d.v2.previewWidth = 512;
    d.v2.previewHeight = 512;
    d.v2.previewFlags = 0x40;
    d.v2.previewKind = 7;
    d.v2.previewFirstMs = 4400;
    d.v2.previewLastMs = 9000;
    d.v2.solvedInPreview = 300;
    d.v2.heroJobsInPreview = 310;
    d.v2.solvedOutside = 5000;
    d.v2.bodyDrawnInPreview = 11;
    d.v2.modelChangesInPreview = 3;
    d.v2.snapsInPreview = 4;
    d.v2.previewInstances = 1;
    d.v2.preview[0] = PreviewInstance{0xabc0, 0x5678, 40, 2, 1, 0x1};
    d.cameraSwitchNow = 1;
    d.cameraSwitchChanges = 4;
    d.cappedFrames = 1;
    d.heroInstance = 0xaaaa;
    d.heroModel = 0xbbbb;
    d.heroRecord = 0xcccc;
    d.instanceChanges = 1;
    d.modelChanges = 2;
    d.snaps = 33;
    d.suitNext = 1;
    d.suit[0] = SuitEvent{77, 2, 0x1111, 0x2222, 9, 0};
    d.fields = 1;
    d.field[0] = Field{inRecord, 0x1f0, 2, 0, 5, 9};
    d.costUsTotal = 2468;
    d.costUsMax = 12.5f;
    d.costUsMean = 2.0f;
    d.sampledDraws = 4000;
    d.sequence = 42;
    FILE* f = std::fopen(path, "wb");
    if (!f)
        return 2;
    const bool ok = std::fwrite(&d, sizeof d, 1, f) == 1;
    std::fclose(f);
    return ok ? 0 : 3;
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 3 && !std::strcmp(argv[1], "--dump"))
        return dump(argv[2]);
    std::puts("mirror probe");
    // 0x2c9c73e..: listed when flags & 7 == 0, or & 6 == 0 with 0x40.
    check(listed(0) && listed(0x480) && !listed(1) && listed(0x41) && !listed(2) && !listed(0x42) && !listed(4) &&
              !listed(0x44),
          "the view list's flag rule (0x2c9c73e)");
    check(builtinActive(0b1011, 4) == 3 && builtinActive(0b1011, 2) == 2 && builtinActive(~0ull, 70) == 64 &&
              builtinActive(5, 0) == 0 && builtinActive(5, -3) == 0,
          "built-in views: the active mask below the count (0x2c9c6f0)");
    const uint64_t slotViews[slots] = {0, 0x500, 0, 0, 0, 0, 0, 0};
    check(classify(0x100, 0, 0x100, 0x200, 0x300, slotViews) == gameView &&
              classify(0x900, 0x200, 0x100, 0x200, 0x300, slotViews) == eyeLeft &&
              classify(0x300, 0, 0x100, 0x200, 0x300, slotViews) == eyeRight &&
              classify(0x500, 0, 0x100, 0x200, 0x300, slotViews) == offscreen &&
              classify(0x901, 0x902, 0x100, 0x200, 0x300, slotViews) == other && classify(0, 0, 1, 2, 3, slotViews) == other,
          "view kinds (render copies by their owner)");
    float hero[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 6, 7, 1};
    float piece[16];
    std::memcpy(piece, hero, sizeof piece);
    piece[12] += .02f;
    float prop[16];
    std::memcpy(prop, hero, sizeof prop);
    prop[13] += .5f;
    float nan[16];
    std::memcpy(nan, hero, sizeof nan);
    nan[12] = std::nanf("");
    check(sameRoot(piece, hero) && !sameRoot(prop, hero) && !sameRoot(nan, hero), "pieces on the hero's root transform");

    // The suit tally: a word that changes with each model change ranks first; noise does not.
    static Tally t;
    uint32_t w[snapWords]{};
    t.step(w, false);
    for (int round = 0; round < 5; ++round) {
        w[3] += 1;              // noise: the instance's word 3 changes every snapshot
        t.step(w, false);
        w[3] += 1;
        w[128 + 10] = 100u + static_cast<uint32_t>(round); // the record's 0x28: with the model only
        t.step(w, true);
    }
    Field top[fieldCount]{};
    const uint32_t n = t.top(top, fieldCount);
    check(n == 1 && top[0].where == inRecord && top[0].offset == 0x28 && top[0].withSuit == 5 && top[0].noise == 0 &&
              top[0].after == 104 && top[0].before == 103,
          "suit tally: the field that changes with the model, not the noise");
    uint16_t where{}, offset{};
    whereOf(0, where, offset);
    const bool w0 = where == inInstance && offset == 0;
    whereOf(64, where, offset);
    const bool w1 = where == inModel && offset == 0;
    whereOf(snapWords - 1, where, offset);
    check(w0 && w1 && where == inRecord && offset == recordBytes - 4, "suit fields: where and offset");
    // top() keeps the best n, best first.
    static Tally many;
    uint32_t v[snapWords]{};
    many.step(v, false);
    for (uint32_t i = 0; i < 40; ++i)
        v[i] = 1;
    many.step(v, true);
    for (uint32_t i = 0; i < 20; ++i)
        v[i] = 2;
    many.step(v, true);
    const uint32_t m = many.top(top, fieldCount);
    bool sorted = m == fieldCount;
    for (uint32_t i = 1; i < m; ++i)
        sorted = sorted && top[i - 1].withSuit * 4 - top[i - 1].noise >= top[i].withSuit * 4 - top[i].noise;
    check(sorted && top[0].withSuit == 2, "suit tally: the best 16, best first");

    // The cost guard: cheap frames keep it on; a slow mean stops it (only after budgetFrames).
    Cost c;
    bool on = true;
    for (uint32_t i = 0; i < budgetFrames * 2; ++i)
        on = on && c.add(5);
    Cost slow;
    bool early = true, later = true;
    for (uint32_t i = 0; i < budgetFrames - 1; ++i)
        early = early && slow.add(500);
    later = slow.add(500);
    check(on && early && !later && c.mean() == 5.f, "cost guard: stops itself above the budget");

    // Cheap: a frame's worst work (a snapshot tally, the top 16, the slot rule) well under the budget.
    static Tally timed;
    static uint32_t words[snapWords];
    const auto t0 = std::chrono::steady_clock::now();
    const int frames = 2000;
    volatile uint32_t sink{};
    for (int i = 0; i < frames; ++i) {
        words[static_cast<uint32_t>(i) % snapWords] ^= 1;
        sink = sink + timed.step(words, (i & 7) == 0);
        Field out[fieldCount];
        sink = sink + timed.top(out, fieldCount);
        for (uint32_t s = 0; s < slots; ++s)
            sink = sink + (listed(static_cast<uint32_t>(i) + s) ? 1u : 0u);
        sink = sink + builtinActive(static_cast<uint64_t>(i) * 2654435761ull, 64);
    }
    const double us =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / frames;
    std::printf("  worst-case frame work: %.2f us (budget %.0f us; real frames tally once in %u)\n", us, budgetUs,
                snapEvery);
    check(us < budgetUs / 2, "cheap: a frame's worst-case work under half the budget");
    check(sizeof(Data) == Data{}.bytes && Data{}.magic == 0x4252504d && Data{}.version == 2, "record size and magic");
    std::printf("%s\n", failures ? "MIRROR PROBE TESTS FAILED" : "all mirror probe tests passed");
    return failures ? 1 : 0;
}
