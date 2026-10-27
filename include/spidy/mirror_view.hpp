#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.3 MIRROR, the furthest safe step toward the real player model (not shipped yet):
//
// 1. SUIT ID WATCH (read-only, always on): the hero instance's word at +0x8c (CANDIDATE suit id:
//    run001's MIRROR PROBE v2 saw it change with all 13 suit changes and never otherwise,
//    0x202b -> 0x2003). A change, held for 3 reads, makes the mirror learn the suit's colours
//    afresh at once and is logged.
// 2. MIRROR VIEW TEST (EXPERIMENTAL, OFF by default; shipped in 0.0.4, needs a restart: the view is
//    only made in the window the eyes are made in, as rep0030 showed the game itself adding and
//    removing a view mid-session with 5 of 8 slots free): a third small game camera (the eyes'
//    own CreateArgs and flags, only with at least 4 of the 8 view slots free) placed 1.2 m in front
//    of the head looking back at the player, for 30 s, then retired (never deleted). It shows
//    nothing in the headset; the MIRROR PROBE counts what the game draws in its slot (is the hero
//    model drawn there?) and the test records its slot, texture and cost. That answers whether a
//    game-rendered mirror is possible; the FIGURE / SUIT mirror stays what you see.
// Pure; the game calls live in stereo_probe.cpp.
#include "spidy/math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace spidy::mirror_view {

inline constexpr uint32_t suitIdAt = 0x8c; // CANDIDATE (hero instance + 0x8c)
// rep0022 (the user's SUITS-menu test, 25 Oct): CONFIRMED that +0x8c is the high half of the hero's
// 64-bit model id at instance +0x88 (all 79 changes; e.g. model 0x0400202b_90400111). Two bits in it
// flip on their own with no suit change (0x2000 and 0x04000000, with 0 changed words); the low
// part changed 0x03 <-> 0x2b with 13-17 changed words (a real model swap). So the watch ignores the
// two flag bits (CANDIDATE: that what is left is the suit).
inline constexpr uint32_t suitFlagBits = 0x04002000u;
inline uint32_t suitPart(uint32_t value) { return value & ~suitFlagBits; }
inline constexpr uint32_t holdReads = 3;

struct SuitWatch {
    uint32_t now{}, pending{}, pendingReads{}, reads{}, failed{}, changes{};
    bool have{};
    // One read; returns true when the suit id changed (the new value held for holdReads reads).
    bool step(bool ok, uint32_t raw) {
        const uint32_t value = suitPart(raw);
        if (!ok) {
            ++failed;
            pendingReads = 0;
            return false;
        }
        ++reads;
        if (!have) {
            have = true;
            now = value;
            return false;
        }
        if (value == now) {
            pendingReads = 0;
            return false;
        }
        if (value != pending) {
            pending = value;
            pendingReads = 1;
        } else {
            ++pendingReads;
        }
        if (pendingReads < holdReads)
            return false;
        now = value;
        pendingReads = 0;
        ++changes;
        return true;
    }
    void forget() { // a new hero: start again (no change counted)
        have = false;
        pendingReads = 0;
    }
};

// The mirror camera: the head's pose turned 180 degrees about the vertical, 1.2 m in front of the
// head (level), looking back at it. pose: 16 floats, rows right / down / forward (world), the
// position at [12..14] (native_view's layout). False when the head pose is unusable.
inline constexpr float mirrorMetres = 1.2f;
// MIRROR TEST (private, bare-bones): the camera stands farther (2 m) and lower (0.8 m below the
// head, about the middle of the body) so the 70 degree portrait frame holds the whole body.
inline constexpr float fullBodyMetres = 2.0f, fullBodyDrop = .8f;
inline bool mirrorPose(const Mat4& head, Mat4& out, float metres = mirrorMetres, float drop = 0.f) {
    for (float v : head)
        if (!std::isfinite(v))
            return false;
    Vec3 forward{head[8], 0, head[10]};
    if (length(forward) < 1e-3f) // looking straight up or down: use the down row's level part
        forward = Vec3{-head[4], 0, -head[6]};
    if (length(forward) < 1e-3f)
        return false;
    forward = normalized(forward);
    out = head;
    for (size_t row = 0; row < 3; ++row) { // 180 degrees about y: x and z change sign
        out[row * 4 + 0] = -head[row * 4 + 0];
        out[row * 4 + 2] = -head[row * 4 + 2];
    }
    out[12] = head[12] + forward.x * metres;
    out[13] = head[13] - drop; // y is up
    out[14] = head[14] + forward.z * metres;
    return true;
}

// The test's course. Each step is a reason the record keeps.
enum Phase : uint32_t { off = 0, waiting, running, done, failed, phases };
inline constexpr const char* phaseNames[phases] = {"off", "waiting", "running", "done", "failed"};
enum Why : uint32_t {
    fine = 0,
    noSlots,      // fewer than 4 free view slots
    noEyes,       // the eyes are not up yet
    createFailed, // the game made no view
    lostView,     // the view left the pool
    tooSlow,      // frames slowed by more than half
    pictureFault, // MIRROR TEST: the picture could not be copied (native_gpu::MirrorGpu::fault)
    whys
};
inline constexpr const char* whyNames[whys] = {"", "fewer than 4 view slots free", "the eyes are not up",
                                               "the game made no view", "the view left the pool",
                                               "too slow: frames over 1.5x", "the picture could not be shown"};
inline constexpr uint32_t minFreeSlots = 4, width = 320, height = 480;
inline constexpr float runSeconds = 30.f, slowFactor = 1.5f, slowAfter = 3.f;
// MIRROR TEST: slow only when also under 50 frames a second (a headset's own pace is not "slow").
inline constexpr float slowFloorMs = 20.f;

struct Test {
    uint32_t phase = off, why = fine, slot = ~0u, runs{};
    float seconds{}, baseMs{}, testMs{};
    uint64_t frames{};
    // MIRROR TEST: true keeps the view while it is wanted (no 30 s end); the player closes it.
    bool keep{true};
    // Whether to create the view now: wanted, eyes up, enough free slots.
    bool shouldCreate(bool wanted, bool eyesUp, uint32_t freeSlots) {
        if (!wanted) {
            if (phase == waiting)
                phase = off;
            return false;
        }
        if (phase == off)
            phase = waiting;
        if (phase != waiting)
            return false;
        if (!eyesUp) {
            why = noEyes;
            return false;
        }
        if (freeSlots < minFreeSlots) {
            why = noSlots;
            phase = failed;
            return false;
        }
        return true;
    }
    void created(bool ok, uint32_t inSlot) {
        if (!ok) {
            phase = failed;
            why = createFailed;
            return;
        }
        phase = running;
        why = fine;
        slot = inSlot;
        seconds = 0;
        testMs = 0;
        frames = 0;
        ++runs;
    }
    // The baseline frame time (before the test), smoothed.
    void idleFrame(float frameMs) {
        if (std::isfinite(frameMs) && frameMs > 0 && frameMs < 200)
            baseMs = baseMs > 0 ? baseMs + .05f * (frameMs - baseMs) : frameMs;
    }
    // One frame of the test; returns true when the view must be retired now.
    bool runFrame(float dt, float frameMs, bool viewOwned) {
        if (phase != running)
            return false;
        if (!viewOwned) {
            phase = failed;
            why = lostView;
            return false; // nothing left to retire
        }
        if (std::isfinite(dt) && dt > 0 && dt < .5f)
            seconds += dt;
        if (std::isfinite(frameMs) && frameMs > 0 && frameMs < 200) {
            testMs = testMs > 0 ? testMs + .1f * (frameMs - testMs) : frameMs;
            ++frames;
        }
        if (seconds > slowAfter && baseMs > 0 && testMs > baseMs * slowFactor && testMs > slowFloorMs) {
            phase = failed;
            why = tooSlow;
            return true;
        }
        if (!keep && seconds >= runSeconds) {
            phase = done;
            return true;
        }
        return false;
    }
    // MIRROR TEST: the player turned it OFF (the view is retired, kept for a reopen).
    void close() {
        if (phase == running)
            phase = done;
    }
    // MIRROR TEST: turned ON again with the view still ours: it runs again (timing afresh).
    bool reopen() {
        if (phase != done)
            return false;
        phase = running;
        why = fine;
        seconds = 0;
        testMs = 0;
        return true;
    }
    // MIRROR TEST: the picture failed (fault from native_gpu): stop, it goes OFF.
    void pictureFailed() {
        if (phase == running || phase == done) {
            phase = failed;
            why = pictureFault;
        }
    }
};
// 0.0.7: the test has run its course (its 30 s done, or stopped by itself): MIRROR VIEW goes back
// to OFF and is saved so, so it runs once and not at every start. Waiting (no eyes yet) is not over.
// MIRROR TEST (private): it stays while ON; only a stop on its own (failed) turns it OFF.
inline bool finished(const Test& t) { return t.phase == failed && t.why != noEyes; }
// Version 0.0.4: what stereo_probe.cpp's test did, copied out for game_xr.cpp's V003 record.
// The view gets none of the eyes' extra flags (no readback, no display output, no GPU slot): the
// game renders it into its own targets, the probe counts the draws in its slot.
inline constexpr const char* viewName = "Spidy mirror test";
inline constexpr float verticalTan = .70f; // a 70 degree tall portrait frustum
inline float horizontalTan() { return verticalTan * width / height; }
struct Stats {
    Test test{};
    uint32_t wanted{}, tooLate{}, freeSlots{}, texture{};
    uint64_t heroDraws{}, otherDraws{}, posed{};
    // MIRROR TEST: the picture (native_gpu::mirrorGpu) - its fault code and copies made.
    uint32_t gpuFault{}, gpuImage{};
    uint64_t gpuCopies{};
};
} // namespace spidy::mirror_view
