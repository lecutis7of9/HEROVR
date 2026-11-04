#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.78 CUT10 GUI2 POI (diagnostic; pure and allocation-free; nothing here reads or writes the game).
// The game's own world-anchored prompts / waypoints / beacons are GUI2 (Coherent Gameface) POI elements owned by one
// static POIView. Static RE of Spider-Man2.exe 2.810.0.0 (CONFIRMED by disassembly, not yet seen in game):
//   0xc699a0 / 0xdf7f90 are the function-local-static getters: POIView lives at image+0xb0f4588 (guard 0xb0f5630),
//   built by 0xc641d0, vtable 0x53cf538; its UIElementPOIManager sits inline at +0x730 (image+0xb0f4cb8), vtable
//   0x53cf408. The manager's per-POI icon records are at +0x58 (stride 0xc4, count dword at +0x64, read by the
//   0xe0ce20 icon pass: +0x18/+0x1c icon atlas ids); records of stride 0x370 at +0x80 (init 0xe073f0).
// world_trace.cpp reads these (SEH-guarded, only when both vtables match exactly) and logs "CUT10 GUI2 POI" lines so a
// report shows which record holds the game's own Y zip prompt / objective waypoint / beacon and its screen transform.
// Made by Agent Mulder.
#include <cmath>
#include <cstdint>
namespace spidy::cut10 {
constexpr uint32_t poiViewRva = 0xb0f4588u, poiViewVtableRva = 0x53cf538u;
constexpr uint32_t managerOffset = 0x730u, managerVtableRva = 0x53cf408u;
constexpr uint32_t iconArray = 0x58u, iconCount = 0x64u, iconStride = 0xc4u;
constexpr uint32_t recordArray = 0x80u, recordCount = 0x8cu, recordStride = 0x370u;
constexpr uint32_t maxRecords = 64u, maxLines = 200u;
// Both tables must match exactly or nothing past them is read (another exe build: one "not this build" line).
inline bool layoutMatches(uint64_t image, uint64_t viewVtable, uint64_t managerVtable) {
    return image && viewVtable == image + poiViewVtableRva && managerVtable == image + managerVtableRva;
}
inline bool countSane(int32_t n) { return n >= 0 && n <= 4096; }
// A world place in a 0x370 record: three finite floats, within 3 km of the hero and not the origin.
inline bool worldTriple(const float* f, float hx, float hy, float hz) {
    if (!std::isfinite(f[0]) || !std::isfinite(f[1]) || !std::isfinite(f[2]))
        return false;
    if (std::fabs(f[0]) + std::fabs(f[1]) + std::fabs(f[2]) < 1.f)
        return false;
    const float dx = f[0] - hx, dy = f[1] - hy, dz = f[2] - hz;
    return dx * dx + dy * dy + dz * dz < 3000.f * 3000.f;
}
// Change signature of one record's words, so a line is written only when it changes.
inline uint64_t signature(const uint32_t* w, uint32_t n) {
    uint64_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < n; ++i)
        h = (h ^ w[i]) * 1099511628211ull;
    return h;
}
} // namespace spidy::cut10
