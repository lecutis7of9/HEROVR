#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 27 / 27.3: ADDRESS FINDER — read-only scan for GlobalWorldCBuffer builders,
// TAA/DLAA jitter/history/mvecs, and (cheap) LOD distance inputs. Candidates
// only; never applied as live addresses. SEH, time and frame caps; on any
// problem the path disables itself and logs one line. Never falls back to
// flat screen and never crashes the session on its own.
// 27.2: heavy work on a background thread; render-thread sampling is budgeted
// (≤0.5 ms / frame) so OpenXR never stalls into flat/black.
// 27.3: raw hits vs confirmed candidates; trivial reject; K-frame confirm;
// frameCount ticks once per noteView (not per barrier); cap never ends the run.
// 28: relative/scaled jitter, view-relative and view*proj forms, lagged cameras,
// near misses, store room for values, one row per site; "almost" list (almost()).
// 30: bind trace of camera data into passes; shadow pass candidates (shadowPasses()).
#include "address_finder.hpp"
#include "native_view.hpp"
#include <cstdint>
#include <d3d12.h>

namespace spidy::native_address_finder {

inline constexpr uint32_t dataMagic = 0x53414644; // SAFD
inline constexpr uint32_t dataVersion = 1;
inline constexpr uint32_t reportBytes = 1024 * 1024; // Update 32: room for ORTHO CAPTURE (was 256 KiB)
inline constexpr uint32_t windowSeconds = 60;
// Update 30.1: a frame here is a stock view update, ~5-7 per game frame (the 28.1 run's cap hit
// after ~26 s), so the cap allows ~70 s at 600 a second; the 60 s window is the real limit.
inline constexpr uint32_t maxFrames = 600 * 70; // hard cap

struct Data {
    uint32_t magic = dataMagic;
    uint32_t version = dataVersion;
    uint32_t bytes = sizeof(Data);
    uint32_t status{}; // 0 idle, 1 running, 2 done, 3 failed
    int64_t sequence{};
    uint32_t secondsLeft{};
    uint32_t candidates{}; // confirmed count (HUD DONE n); not raw hits
    uint32_t generation{};
    uint32_t fault{}; // 0 ok; else a one-shot code (see native_address_finder.cpp)
    uint64_t frames{};
    uint64_t scans{};
    uint64_t exportHits{};
    uint64_t stringHits{};
    uint32_t lodOffset = UINT32_MAX; // byte offset in active view, or UINT32_MAX
    uint32_t rawHits{}; // Update 27.3: stored raw hits (cap applies here; never ends run)
};
static_assert(sizeof(Data) == 80);

// 1 starts (or restarts) a ~60 s window; 0 stops early and finalises. Returns
// 0 on accept, else a refuse code (finder already failed hard, bad arg, …).
uint32_t set(uint32_t on);
uint32_t status();
// View fields for the hologram: status, seconds left (or confirmed count when done).
void viewFields(uint32_t& statusOut, uint32_t& secondsOrCandidates);
// Update 28: the last report's "almost" rows (not confirmed; HUD DONE n +m ALMOST).
uint32_t almost();
// Update 30: depth-only passes that bound this session's camera data in the last
// report (SHADOW PASS CANDIDATES; HUD DONE n +m ALMOST, k SHADOW).
uint32_t shadowPasses();

// Callouts from existing hooks (no second hook on the same targets).
void onBarrier(ID3D12GraphicsCommandList* list, UINT count, const D3D12_RESOURCE_BARRIER* bs);
void noteView(unsigned kind, const native_view::Descriptor& d); // 0 stock, 1 aligned head, 2/3 eyes (Update 29)

} // namespace spidy::native_address_finder
