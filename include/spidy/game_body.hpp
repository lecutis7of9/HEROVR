#pragma once
// Update 0.0.72 GAME BODY (EXPERIMENTAL): the parts of "the game's own hero
// model as the VR body" that need no game: the hero rig's names and table,
// the strict gate the body waits behind, and the byte searches that find the
// game's functions again on another build. Engine independent and tested in
// tests/core_tests.cpp; src/native_game_body.cpp feeds it from the game.
//
// Every game address the body uses is CANDIDATE (Spider-Man 2 2.810, from the
// multiplayer team's notes and SM2VR 0.0.27): none is trusted until its bytes
// and the live checks here pass, every session.
#include "body_ik.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace spidy::game_body {
// CANDIDATE addresses (RVAs into the game's exe), Spider-Man 2 2.810.0.0.
inline constexpr uintptr_t candidateRegistry = 0xc2d3d20;      // entity registry; count at +0x20
inline constexpr uintptr_t candidateHeroLocalVtable = 0x5424128;
inline constexpr uintptr_t candidateMoverManagerVtable = 0x5438988; // +0xdcc: the standard mover's handle
inline constexpr uintptr_t candidateMoverStandardVtable = 0x85d24d8;
inline constexpr uintptr_t candidateWriter = 0x292b880;  // the hero pose job's writer
inline constexpr uintptr_t candidateToModel = 0x292af50; // local pose -> model space (rest pose)
// Their first bytes, as SM2VR 0.0.27 and the team read them.
inline constexpr std::array<uint8_t, 20> writerPrologue{0x48, 0x89, 0x5C, 0x24, 0x08, 0x55, 0x56, 0x57, 0x41, 0x54,
                                                        0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x20};
inline constexpr std::array<uint8_t, 11> toModelPrologue{0x48, 0x8B, 0xC4, 0x57, 0x48, 0x81,
                                                         0xEC, 0xA0, 0x00, 0x00, 0x00};
// Rig layout: joint count u16 at +2, joint table at +8 (16 bytes a joint:
// parent s16 at +0, name hash u32 at +8, name offset at +0xc), rest table at
// +0x18 (48 bytes a joint).
inline constexpr uint32_t rigCount = 2, rigJoints = 8, rigRest = 0x18, jointBytes = 16, restBytes = 48;
inline constexpr uint32_t maxJoints = 512;

// The game's name hash: CRC-32 table steps seeded with 0xedb88320, no final
// inversion ("pelvis" is 0x8e59b33f).
uint32_t nameHash(const char* name);

struct Table {
    std::vector<int16_t> parent;
    std::vector<uint32_t> hash;
};
// Parses `count` joint entries; false unless every parent comes before its
// child (the game's conversion needs that) or is -1.
bool parseJoints(std::span<const uint8_t> bytes, uint32_t count, Table& out);
// A reference local-to-model conversion for the tests (the game's own,
// candidateToModel, builds the real rest pose): each 48-byte rest entry is a
// scale (x, y, z, w), a rotation quaternion (x, y, z, w), a translation
// (x, y, z, w), local to the parent. Out: 16 floats a joint (body_ik layout).
bool restToModel(std::span<const uint8_t> rest, const Table& table, std::vector<float>& out);

int byName(const Table&, const char* name);
// The joints from `from` (exclusive) up to `to` (inclusive), bottom first;
// empty when `to` is not under `from`.
std::vector<int16_t> chain(const Table&, int from, int to);

// Roles from the rig's joint names. `missing` names each required joint
// that is not there (and "spine"/"neck" when the chains do not connect);
// the body refuses the rig unless it is empty.
bool identify(const Table&, body::Rig& rig, std::vector<std::string>* missing = nullptr);
// Joints the body must never move, which gameplay reads: fx_root, sync,
// cameraTarget, aimTarget and the IK targets (the rig's other roots).
// The IK names are CANDIDATE (the spec says "*IK"; the rig stores only hashes,
// so a suffix can't be matched): the likely ones are listed, and any other
// root is caught by the root rule below.
inline constexpr const char* protectedNames[] = {"fx_root",   "sync",      "cameraTarget", "aimTarget",
                                                 "LF_handIK", "RT_handIK", "LF_footIK",    "RT_footIK"};
// True when none of the protected joints present, and no root of the rig but
// the body's own, hangs under a joint the solve moves (the body root and
// everything under it, the arms and legs). `failed` gets the first that does.
bool protectedClear(const Table&, const body::Rig&, std::string* failed = nullptr);

// The gate: the body waits until every check passed `needed` times in a row
// on the same player record; any failed check or another record drops it
// at once.
struct Gate {
    uint32_t needed = 3;
    uintptr_t record{};
    uint32_t passes{};
    bool verified{};
    std::string failed; // the first failed check of the latest round
    // One round of checks; returns verified().
    bool feed(uintptr_t playerRecord, const char* failedCheck);
    bool open(uintptr_t playerRecord) const {
        return verified && playerRecord && playerRecord == record;
    }
};

// Byte searches over the game's mapped image (the whole image from its base).
struct Section {
    uint32_t rva{}, size{};
    bool code{};
};
// The PE's sections; empty when the headers do not read as a PE.
std::vector<Section> sections(std::span<const uint8_t> headers);
bool matches(std::span<const uint8_t> image, uintptr_t rva, std::span<const uint8_t> bytes);
// Every RVA where `bytes` start, inside the given sections (at most `limit`).
std::vector<uintptr_t> findAll(std::span<const uint8_t> image, std::span<const Section> where,
                               std::span<const uint8_t> bytes, size_t limit = 64);
// True when the function at `from` calls or jumps (E8/E9 rel32) to `to`
// within its first `length` bytes.
bool calls(std::span<const uint8_t> image, uintptr_t from, uintptr_t to, size_t length = 0x400);
// The vtable of the MSVC RTTI type `mangled` (".?AVHeroLocal@Hero@@"):
// the type descriptor, its complete-object locator (signature 1, offset 0,
// its own RVA at +20), the image pointer to that locator and the vtable
// right after it. 0 unless exactly one.
uintptr_t vtableByRtti(std::span<const uint8_t> image, uint64_t imageBase, const char* mangled);
// Reads `n` bytes at an RVA of the game's image; false where it cannot.
using Reader = std::function<bool(uintptr_t rva, void* out, size_t n)>;
// True when the vtable at `vtableRva` is the one MSVC RTTI names a class
// starting with `prefix` (".?AVHeroLocal@Hero@@", ".?AVMoverStandard@"): the
// pointer before it is its complete-object locator (signature 1, offset 0,
// its own RVA at +20), whose type descriptor's name (+0x10) starts so.
bool rttiNamed(const Reader& read, uint64_t imageBase, uintptr_t vtableRva, const char* prefix);
// The writer and conversion pair: the candidates when their prologues match
// and the writer calls the conversion; otherwise the one place where a
// writer prologue calls a conversion prologue. False unless exactly one.
struct Functions {
    uintptr_t writer{}, toModel{};
    bool refound{};
};
bool findFunctions(std::span<const uint8_t> image, std::span<const Section> code, Functions& out);
// The candidates alone, through a reader (no whole-image view needed).
bool candidatesHold(const Reader& read);
} // namespace spidy::game_body
