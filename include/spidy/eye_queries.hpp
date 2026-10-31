#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.73 LIGHT CHECKS: room in each eye's GPU visibility-query pool for the street lights' glows.
// Exe (Spider-Man2.exe 2.810.0.0, by disassembly; docs/LIGHTS-0.0.73.md):
// - CONFIRMED: the halo / streak sprites of street and traffic lights (VFX 0x2deecd0) ask
//   RequestRenderQuery 0x2c8be20 twice per light (wrapper 0x2deb3c0, flags 0x81c and 0xa0c) in
//   the drawing view's pool (view+0x1c78), and the consumer 0x2def7ac..0x2def92c hides the sprite
//   whenever either answer is missing or zero (intensity = (seen / total)^2).
// - CONFIRMED: 0x2c8be20 answers 0 when the pool is full ([mgr+0x88] capacity minus [mgr+0x8c]
//   used < 1, bitset 0x3131fb0 / 0x3131810); the model occlusion queries share the same pool;
//   slots idle 6 frames are freed (0x2c8a540) and a new slot starts unanswered.
// - CONFIRMED: the view constructor 0x2c98350 sizes the pool (0x2c8a810 at 0x2c98a62) from its
//   cap argument (negative: the game's default [0xc35950c]) clamped by the view kind: kind 2
//   (our eyes: CreateArgs b = 2 through 0x2c64ce5's mapping) to 0x100 by `mov eax, 0x100` at
//   0x2c98a41 (b8 00 01 00 00); the game's own view (kind 0) is not clamped.
// - CANDIDATE: that 256 is too few for both models and every light at night, so lights strobe as
//   their checks fail or restart (every version since SpidyVR's design made the eyes kind-2 views).
// ON: while an eye is created, the clamp's immediate becomes this room (one byte, 0x2c98a43) and
// the eye asks for it; put back straight after. OFF: the game's 256. Applies when the eyes are
// made (the next VR start). Bytes that differ: nothing is written (the game's 256).
#include <array>
#include <cstdint>

namespace spidy::eye_queries {

inline constexpr uintptr_t clampRva = 0x2c98a41;
inline constexpr uint8_t clampBytes[5] = {0xb8, 0x00, 0x01, 0x00, 0x00};
inline constexpr uintptr_t clampByteRva = clampRva + 2; // the 0x01 of 0x100
inline constexpr uint32_t stockRoom = 0x100;
inline constexpr uint32_t defaultRoom = 0x400; // kind 1 views' own clamp in 0x2c98350
inline constexpr uint32_t mostRoom = 0x1000;
// The view's query pool (view+0x1c78) and its bitset's capacity / used counts.
inline constexpr uintptr_t poolOffset = 0x1c78, capacityOffset = poolOffset + 0x88, usedOffset = poolOffset + 0x8c;

// The wrist value: 0 ON (the default), 1 OFF.
inline bool on(uint32_t value) { return value == 0; }
// The room each eye gets: at least the game view's own pool (read from it), at least 1024, at most
// 4096, in whole 256s (so only the immediate's second byte changes).
inline uint32_t room(int32_t gameViewCapacity) {
    uint32_t r = defaultRoom;
    if (gameViewCapacity > 0 && static_cast<uint32_t>(gameViewCapacity) > r)
        r = static_cast<uint32_t>(gameViewCapacity);
    if (r > mostRoom)
        r = mostRoom;
    return (r + 0xff) & ~0xffu;
}
inline uint8_t roomByte(uint32_t r) { return static_cast<uint8_t>(r >> 8); }

} // namespace spidy::eye_queries
