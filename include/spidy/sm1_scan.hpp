// HERO VR 0.0.76 SM1: the research scan (sm1scan). Made by Agent Mulder.
// Mulder's Heroes add-on for iilyn's Spider-Man VR mod (SpidyVR by Ilya Mezerowsky, GPL-3.0,
// https://github.com/IlyaMez/SpidyVR).
//
// One test session on Spider-Man.exe 4.630.0.0 (Steam 1817070) collects what the next SM1 features need, and
// nothing else: held props (the hand joints' pose and where an object would attach), the enemies (list, position,
// alive state, what changes when a blow lands), the grapple anchors (the game's rope target points and the world
// query), and spawning / seating (the game's RTTI classes for vehicles, seats and spawners).
//
// STRICTLY READ-ONLY. The scan never writes game memory, never calls a game function and never calls one of his
// functions either: it copies memory (each copy behind VirtualQuery and an SEH guard) and his exported *data*
// (SpidyXrData, SpidyStrikeData, SpidyMotionData, SpidyBodyPoses...). It runs on one lowest-priority thread,
// sleeps between small chunks, stops by itself after `Limits::runMs` and writes only under
// %APPDATA%\HERO VR SM1\scan. Any fault ends the scan and nothing else.
//
// Every address below is CANDIDATE: an RVA from SpidyVR 0.7.0's own sources (file named per entry) for the build he
// targets, 4.630.0.0. The scan records what is really there (bytes, pointers, RTTI names) so each one can be
// checked offline; a match with his expected bytes is recorded, it does not make the address CONFIRMED by itself.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace spidy::sm1::scan {

enum class Area : uint8_t { registry, player, hands, enemies, damage, grapple, physics, spawning, count };
const char* areaName(Area a);
enum class Kind : uint8_t {
    code,   // a function: its first bytes
    data,   // a global: its 8 bytes, and what they point at
    vtable, // a vtable: its RTTI class name and its first virtual functions
};
const char* kindName(Kind k);

struct Anchor {
    const char* name;
    uint32_t rva;
    Kind kind;
    Area area;
    const char* source; // where in SpidyVR 0.7.0 it comes from
    const uint8_t* expect = nullptr; // his expected first bytes (his own entry check), when he has one
    uint8_t expectBytes = 0;
};
const std::vector<Anchor>& anchors();

// The game's name hash (his native_body.cpp nameHash, the game's 1bb88d0): CRC-32 table steps from 0xedb88320,
// no final inversion.
uint32_t jointHash(std::string_view name);
// The joints the hands need (his native_body.cpp names them; LF_/RT_ prefixes).
const std::vector<std::string>& jointNames();

// An MSVC RTTI type name -> the class name: ".?AVHeroHealth@@" -> "HeroHealth", ".?AVBar@Foo@@" -> "Foo::Bar".
// Empty when it is not one (templates and other decorations are kept as written after the prefix).
std::string className(std::string_view decorated);
// The scan's interest in a class name: true and its area when it names something the scan looks for.
bool interesting(std::string_view name, Area& area);
// An enemy's components (his game_targets.cpp RTTI names): a bot's mover manager or a character bot class.
bool botClass(std::string_view name);
// Dump mode (0.0.76): the suit / loadout classes the scan watches before and after a suit change.
// SkinManager, SunsetVanityEquippedInventory, ItemLoadoutComponent, HeroCharacterManager (exact class or a namespaced one).
bool suitClass(std::string_view name);
// Dump mode (0.0.76): a vehicle's classes ("Vehicle", "Traffic", "Driv..." in the name; not "Card"/"Carried").
bool vehicleClass(std::string_view name);

// His actor transform check (game_player.cpp transform): finite, |v| < 1e7, orthonormal rows.
bool actorTransform(const float m[16]);

std::string hex(const void* p, size_t n);
std::string quoted(std::string_view s); // a JSON string
std::string number(double v);           // a JSON number (null when not finite)

// The game's component handle (game_player.cpp component): generation << 20 | index.
inline constexpr uint32_t handleOf(uint32_t generation, uint32_t index) { return generation << 20 | (index & 0xfffff); }

// Where a pattern occurs in a chunk read at `at` (signature scan); at most maxHits in `hits`.
void findAll(const uint8_t* data, size_t n, uint64_t at, const uint8_t* pattern, size_t patternBytes,
             std::vector<uint64_t>& hits, size_t maxHits);

struct Limits {
    uint32_t startDelayMs = 15000;      // after his VR starts, before anything is read
    uint32_t snapshotMs = 2000;         // one snapshot
    uint32_t eventPollMs = 250;         // a blow landed / a web attached: an extra snapshot
    uint32_t censusMs = 120000;         // the registry walked again (0.0.76: every 2 minutes, was 30 s)
    uint32_t runMs = 15 * 60 * 1000;    // the whole scan, after the player is found
    uint64_t maxFileBytes = 48ull << 20; // the session's .jsonl and .bin together
    uint32_t maxBots = 48, maxRopeDumps = 120, keepSessions = 5;
    uint32_t chunkBytes = 64 << 10;     // read between sleeps (signature and RTTI passes; 0.0.76: was 256 KB)
    uint32_t chunkEntries = 512;        // registry entries between sleeps (0.0.76: was 2048)
    uint32_t breatheMs = 5;             // the sleep between chunks (0.0.76: was 1 ms)
    uint32_t suitWatchBytes = 0x600;    // dump mode: each suit/loadout component's bytes compared per snapshot
    uint32_t maxSuitDumps = 400, maxSuitDumpsEach = 40;
    float vehicleNearMeters = 25;       // dump mode: a vehicle record's components when the player is this close
    uint32_t maxVehicleDumps = 200;
    uint32_t eyeTargetsMs = 60000;      // dump mode: each eye's render target set again after this long
    uint32_t maxEyeTargetPasses = 6;    // dump mode: passes per session (both eyes each pass)
};
// "sm1scan-20261101-013000".
std::string sessionName(int year, int month, int day, int hour, int minute, int second);
// Old sessions to delete when a new one starts: every sm1scan-*.jsonl/.bin past the newest `keep` session names.
std::vector<std::string> oldSessions(std::vector<std::string> files, uint32_t keep);

// Offsets in his exported data (his 0.7.0 headers; the layer static_asserts them against its copies).
inline constexpr size_t xrHeadAt = 96, xrHandsAt = 160, xrPlayerRecordAt = 608, xrDataBytes = 1312;
inline constexpr size_t motionPositionAt = 64, motionGroundedAt = 168, motionDataBytes = 176;
inline constexpr size_t strikeLandedAt = 32, strikeEnemiesAt = 132, strikeTargetAt = 136, strikeCueTargetAt = 240,
                        strikeCuePointAt = 248, strikeLastPointAt = 164, strikeLastDamageAt = 192,
                        strikeKnockoutsAt = 280, strikeBytes = 328;
// native_body::Poses v1: joints at 12, sequence 16, rig 24, instance 32, out 40, captured 48, transform 56, game 120.
inline constexpr size_t posesJointsAt = 12, posesRigAt = 24, posesInstanceAt = 32, posesCapturedAt = 48,
                        posesTransformAt = 56, posesGameAt = 120, posesMaxJoints = 256;

// Dump mode (0.0.76): each eye's render target set, to find where the eye's depth is.
// His SpidyStereoData (spidy_stereo_probe.dll, stereo_probe.cpp Data v4, CONFIRMED in his 0.7.0 sources): magic,
// version, bytes, state; sequence (a seqlock: odd while he writes) at 16; eyes[2] at 64, 144 bytes each: view +0,
// texture +8, textureObject +16, width +36, height +40, renderWidth +128, renderHeight +132.
inline constexpr size_t stereoSequenceAt = 16, stereoEyesAt = 64, stereoEyeBytes = 144, stereoDataBytes = 352;
inline constexpr size_t eyeViewAt = 0, eyeWidthAt = 36, eyeHeightAt = 40, eyeRenderWidthAt = 128, eyeRenderHeightAt = 132;
// The game side, CANDIDATE (RVAs and offsets from his stereo_probe.cpp and VALIDATION.md for 4.630.0.0):
// the eye view's buffer set is inline at view +0x1f80 (his initBuffers: view = buffers - 0x1f80); its records from
// the set's +0x14a0, 0x1e8 bytes each, record i is the target table's entry i (0x30 bytes each at RVA 0x609b060),
// 132 of them; a record's size (two u32) at +0x1cc.
// His eye colour chain: texture = *(view +0x1f70), object = *(texture +0x40) whose vtable is RVA 0x4e56c80,
// the ID3D12Resource = *(*(object +0x38)).
inline constexpr uint32_t eyeSetAt = 0x1f80, setRecordsAt = 0x14a0, setRecordBytes = 0x1e8, setRecordSizeAt = 0x1cc,
                          setRecordCount = 132, targetTableRva = 0x609b060, targetEntryBytes = 0x30;
inline constexpr uint32_t eyeTextureAt = 0x1f70, textureObjectAt = 0x40, textureObjectVtableRva = 0x4e56c80,
                          objectResourceAt = 0x38;
// Record i's offset from the eye view.
inline constexpr uint32_t setRecordOffset(uint32_t i) { return eyeSetAt + setRecordsAt + i * setRecordBytes; }
// A DXGI format a depth buffer can have: D32_FLOAT, R32_TYPELESS, D24_UNORM_S8_UINT, R24G8_TYPELESS,
// D32_FLOAT_S8X24_UINT, R32G8X24_TYPELESS, D16_UNORM, R16_TYPELESS.
bool depthFormat(uint32_t dxgi);
// Its name ("D32_FLOAT"...), or "" for a format that is not one of those.
const char* depthFormatName(uint32_t dxgi);
// One target's verdict: "depthAtEyeSize" (a depth format at the eye's render size), "depthOtherSize",
// "atEyeSize" (another format at the eye's size) or "other".
const char* eyeTargetVerdict(uint32_t dxgi, uint64_t width, uint32_t height, uint32_t renderWidth, uint32_t renderHeight);
// A record across the session: how often it was a depth target at the eye's size (each pass counts each eye),
// and whether both eyes ever had the same resource in it.
struct EyeDepthTally {
    uint32_t index = 0;
    std::string name;
    uint32_t atEyeSize = 0;
    bool shared = false;
};
// The session's verdict: "CONFIRMED record N (name)" when exactly one record was a depth target at the eye's size on
// both eyes in every pass (at least 2 passes), with a resource per eye; "CANDIDATE records ..." when some were;
// "not found" otherwise.
std::string eyeDepthStatus(const std::vector<EyeDepthTally>& tally, uint32_t passes);

// The layer's side (Windows only; sm1_scan.cpp). Started once, after his VR session starts.
struct Feed {
    bool (*enabled)() = nullptr;           // live.txt sm1scan
    void (*log)(const char* line) = nullptr; // layer.log
    // The vtable of an ID3D12Resource the runtime made (one of his swapchain images), 0 when none yet. The eye
    // target pass calls ID3D12Resource::GetDesc only on objects with exactly this vtable.
    uintptr_t (*resourceVtable)() = nullptr;
};
void start(const Feed& feed);
} // namespace spidy::sm1::scan
