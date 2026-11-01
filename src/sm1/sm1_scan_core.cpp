// HERO VR 0.0.76 SM1: the research scan's pure part (tested on Linux). Made by Agent Mulder.
// The RVAs are SpidyVR 0.7.0's (Ilya Mezerowsky, GPL-3.0, https://github.com/IlyaMez/SpidyVR), all CANDIDATE.
#include "spidy/sm1_scan.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace spidy::sm1::scan {
namespace {
// His own entry checks (the bytes he requires before he hooks), copied from his sources.
constexpr uint8_t ropeUpdateBytes[] = {0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10, 0x48,
                                       0x89, 0x70, 0x20, 0x55, 0x57, 0x41, 0x54, 0x41};
constexpr uint8_t ensureRopeBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x10, 0x57, 0x48, 0x83,
                                       0xec, 0x20, 0x48, 0x8b, 0xf9, 0x45, 0x8b, 0xd8};
constexpr uint8_t ropeTargetBytes[] = {0x0f, 0xb7, 0xc2, 0x83, 0xf8, 0x10, 0x73, 0x56,
                                       0x4c, 0x69, 0xc8, 0xc8, 0x07, 0x00, 0x00, 0x41};
constexpr uint8_t releaseRopeBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83,
                                        0xec, 0x20, 0x44, 0x8b, 0x1a, 0x41, 0x0f, 0xb6};
constexpr uint8_t ropeGetBytes[] = {0x0f, 0xb7, 0xc2, 0x83, 0xf8, 0x10, 0x73, 0x30,
                                    0x4c, 0x69, 0xc0, 0xc8, 0x07, 0x00, 0x00, 0x41};
constexpr uint8_t handPointsBytes[] = {0x48, 0x8b, 0xc4, 0xf3, 0x0f, 0x11, 0x50, 0x18,
                                       0x55, 0x53, 0x56, 0x41, 0x56, 0x48, 0x8d, 0xa8};
constexpr uint8_t poseWriterBytes[] = {0x48, 0x89, 0x5c, 0x24, 0x08, 0x55, 0x56, 0x57, 0x41, 0x54,
                                       0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xec, 0x20};
constexpr uint8_t poseModelBytes[] = {0x48, 0x8b, 0xc4, 0x57, 0x48, 0x81, 0xec, 0xa0, 0x00, 0x00, 0x00};
constexpr uint8_t setTransformBytes[] = {0x40, 0x53, 0x48, 0x83, 0xec, 0x40, 0x0f, 0x10, 0x02, 0x33, 0xc0};

template <size_t N> Anchor expecting(Anchor a, const uint8_t (&bytes)[N]) {
    a.expect = bytes;
    a.expectBytes = static_cast<uint8_t>(N);
    return a;
}
} // namespace

const char* areaName(Area a) {
    switch (a) {
    case Area::registry: return "registry";
    case Area::player: return "player";
    case Area::hands: return "hands";
    case Area::enemies: return "enemies";
    case Area::damage: return "damage";
    case Area::grapple: return "grapple";
    case Area::physics: return "physics";
    case Area::spawning: return "spawning";
    default: return "?";
    }
}
const char* kindName(Kind k) {
    switch (k) {
    case Kind::code: return "code";
    case Kind::data: return "data";
    case Kind::vtable: return "vtable";
    }
    return "?";
}

const std::vector<Anchor>& anchors() {
    using K = Kind;
    using A = Area;
    static const std::vector<Anchor> list = {
        // The component registry (game_player.cpp, game_targets.cpp, native_rays.cpp, game_warp.cpp).
        {"registryTable", 0x7a44320, K::data, A::registry, "game_player.cpp registryTable"},
        {"registryCount", 0x7a44340, K::data, A::registry, "game_player.cpp registryCount"},
        {"actorRecords", 0x7a44380, K::data, A::registry, "game_throw.cpp actorTable (0xc0-byte records)"},
        {"actorRecordCount", 0x7a4439c, K::data, A::registry, "game_throw.cpp actorCount"},
        {"registryDecode", 0x16798f0, K::code, A::registry, "game_player.hpp (the handle decode)"},
        {"findInTable", 0x16799a0, K::code, A::registry, "native_bodies.cpp findInTableRva (record +0x80 table)"},
        {"findInList", 0x167ab40, K::code, A::registry, "native_bodies.cpp findInListRva (record +0x58 list)"},
        // The player (game_player.cpp).
        {"heroLocal", 0x38a93c8, K::vtable, A::player, "game_player.cpp heroLocal"},
        {"heroMover", 0x38b2c98, K::vtable, A::player, "game_player.cpp heroMover (+0xdb4 mover handle, +0xd9c profile)"},
        {"moverStandard", 0x4f70168, K::vtable, A::player, "game_player.cpp moverStandard"},
        {"heroHealth", 0x38a6df0, K::vtable, A::player, "game_player.cpp heroHealthTable (HP float +0x88)"},
        {"heroTransition", 0x38b4350, K::vtable, A::player, "game_warp.cpp managerVtable (HeroTransitionManager)"},
        {"stateMachine", 0x4f84148, K::vtable, A::player, "game_warp.cpp machineVtable (SyncStateMachine)"},
        // The hands (native_body.cpp, native_webs.cpp).
        expecting({"poseWriter", 0x1601290, K::code, A::hands, "native_body.cpp writerBytes (model-space joints)"},
                  poseWriterBytes),
        expecting({"poseModel", 0x1600770, K::code, A::hands, "native_body.cpp modelBytes"}, poseModelBytes),
        {"nameHash", 0x1bb88d0, K::code, A::hands, "native_body.cpp nameHash (the joint name hash)"},
        expecting({"heldHandPoints", 0x95f9c0, K::code, A::hands, "native_webs.cpp handPointsBytes (held web points)"},
                  handPointsBytes),
        expecting({"instanceSetTransform", 0x191c0e0, K::code, A::hands, "stereo_probe.cpp setTransformBytes"},
                  setTransformBytes),
        // The enemies (game_targets.cpp, game_throw.cpp, game_sense.cpp).
        {"bodyCapsule", 0x1fbba90, K::code, A::enemies, "game_targets.hpp (capsule from MoverManager +0xdb8..)"},
        {"throwHandle", 0x957620, K::code, A::enemies, "game_throw.cpp handleRva"},
        {"throwPart", 0x957640, K::code, A::enemies, "game_throw.cpp partRva"},
        {"throwWay", 0x9575d0, K::code, A::enemies, "game_throw.cpp wayRva"},
        // Damage (native_bodies.cpp).
        {"damageSystem", 0x62a4ec0, K::data, A::damage, "native_bodies.cpp damageSystem"},
        {"directDamage", 0x1eb6d60, K::code, A::damage, "native_bodies.cpp directDamageRva"},
        {"addStatus", 0x1ed20e0, K::code, A::damage, "native_bodies.cpp addStatusRva"},
        {"attackResponses", 0x6c74b40, K::data, A::damage, "native_bodies.cpp attackResponses (type)"},
        {"attackResponsesName", 0x3836df0, K::data, A::damage, "native_bodies.cpp attackResponsesName"},
        // The grapple (native_webs.cpp, native_rays.cpp, game_shooter.cpp).
        {"heroRopeManager", 0x38b3df8, K::vtable, A::grapple, "native_webs.hpp (16 RopeManager slots, 0x7c8 each)"},
        expecting({"ropeUpdate", 0x676dd0, K::code, A::grapple, "native_webs.cpp updateBytes"}, ropeUpdateBytes),
        expecting({"ensureRope", 0x677d20, K::code, A::grapple, "native_webs.cpp ensureBytes"}, ensureRopeBytes),
        expecting({"setRopeTarget", 0x67d7c0, K::code, A::grapple, "native_webs.cpp targetBytes"}, ropeTargetBytes),
        expecting({"releaseRope", 0x67b610, K::code, A::grapple, "native_webs.cpp releaseBytes"}, releaseRopeBytes),
        expecting({"ropeGet", 0x67b7b0, K::code, A::grapple, "native_webs.cpp getBytes"}, ropeGetBytes),
        {"ropeTargetGet", 0x6786c0, K::code, A::grapple, "native_webs.hpp (returns rope +0x67c)"},
        {"worldQuery", 0x2e67010, K::code, A::grapple, "native_rays.cpp (the world ray query he hooks)"},
        {"queryCallerSync", 0x18173d1, K::code, A::grapple, "native_rays.cpp (synchronous caller)"},
        {"queryCallerWorker", 0x18180fe, K::code, A::grapple, "native_rays.cpp (worker caller)"},
        {"rayCollector", 0x3d0a948, K::vtable, A::grapple, "native_rays.cpp (closest-hit collector vtable)"},
        {"queryMarker", 0x3d0a9c8, K::data, A::grapple, "native_rays.cpp (query object tag)"},
        {"collectorInit", 0x6b14b40, K::data, A::grapple, "native_rays.cpp (16 bytes copied into a collector)"},
        {"webShooter", 0x391c950, K::vtable, A::grapple, "game_shooter.cpp webShooter"},
        {"shotWebShooter", 0x3907d30, K::vtable, A::grapple, "game_shooter.cpp shotWebShooter"},
        // Physics (native_bodies.cpp, native_rays.cpp, game_time.hpp).
        {"worldGlobal", 0x78939e8, K::data, A::physics, "native_bodies.cpp worldGlobal (the query's world)"},
        {"physicsGlobal", 0x609a570, K::data, A::physics, "native_bodies.cpp physicsGlobal (+0x1ea98 records)"},
        {"physicsStep", 0x609a560, K::data, A::physics, "native_bodies.hpp (Havok step length)"},
        {"collisionProfiles", 0x7813120, K::data, A::physics, "native_rays.cpp profilesRva (32-byte profiles)"},
        {"collisionFilter", 0x18060a0, K::code, A::physics, "native_rays.hpp (InsomniacCollisionFilter)"},
        {"physicsComponent", 0x3d0a460, K::vtable, A::physics, "native_bodies.cpp physicsComponent"},
        {"physicsActors", 0x7a436e8, K::data, A::physics, "native_bodies.cpp actorTable"},
        {"physicsActorCount", 0x7a43704, K::data, A::physics, "native_bodies.cpp actorCount"},
        {"preCollide", 0x2e54300, K::code, A::physics, "native_bodies.cpp preCollideRva"},
        {"setFreebody", 0x1810530, K::code, A::physics, "native_bodies.cpp setFreebodyRva"},
        {"setMode", 0x1835780, K::code, A::physics, "native_bodies.cpp setModeRva"},
        {"rebuild", 0x1830930, K::code, A::physics, "native_bodies.cpp rebuildRva"},
        {"rebuildAsset", 0x1834b70, K::code, A::physics, "native_bodies.cpp rebuildAssetRva"},
        {"setBodyVelocity", 0x2e48b80, K::code, A::physics, "native_bodies.cpp setVelocityRva"},
        {"setBodySpin", 0x2e48830, K::code, A::physics, "native_bodies.cpp setSpinRva"},
        {"postStep", 0x182d230, K::code, A::physics, "native_bodies.hpp (PostStep moves the instance)"},
        {"pickUpThrow", 0x6a1620, K::code, A::physics, "native_bodies.hpp (the game's pick-up-and-throw)"},
        {"flungState", 0x3832fd8, K::vtable, A::physics, "native_bodies.cpp flungState"},
        {"flungDriver", 0x386c448, K::vtable, A::physics, "native_bodies.cpp flungDriver"},
        {"stateMachineData", 0x4f843d0, K::data, A::physics, "native_bodies.cpp stateMachine"},
        // Spawning (game_shooter.cpp: the one spawn path he uses, a shot).
        {"spawnShot", 0x2150830, K::code, A::spawning, "game_shooter.cpp spawnShotRva"},
        {"newShotId", 0x215f040, K::code, A::spawning, "game_shooter.cpp newShotIdRva"},
        {"makeActorRef", 0x1f7b8e0, K::code, A::spawning, "game_shooter.cpp makeRefRva"},
        {"refRecord", 0x1f7b760, K::code, A::spawning, "game_shooter.cpp refRecordRva"},
        {"shotTable", 0x656d9f0, K::data, A::spawning, "game_shooter.cpp shotTable"},
        {"modelOverride", 0x1776050, K::code, A::spawning, "native_bodies.cpp modelOverrideRva"},
        {"requestState", 0x20e51c0, K::code, A::spawning, "native_bodies.cpp requestStateRva"},
    };
    return list;
}

uint32_t jointHash(std::string_view name) {
    static const auto table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (c >> 1) ^ 0xedb88320u : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    uint32_t c = 0xedb88320u;
    for (const char ch : name)
        c = table[(c ^ static_cast<uint8_t>(ch)) & 0xff] ^ (c >> 8);
    return c;
}

const std::vector<std::string>& jointNames() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> v = {"a_body", "pelvis", "chest", "head"};
        for (const char* side : {"LF_", "RT_"})
            for (const char* j : {"clavicle", "uparm", "loarm", "wrist", "finger_B_B", "thumb_A", "finger_A_A",
                                  "finger_C_A", "finger_D_A"})
                v.push_back(std::string(side) + j);
        return v;
    }();
    return names;
}

std::string className(std::string_view d) {
    if (d.size() < 6 || d.substr(0, 3) != ".?A" || (d[3] != 'V' && d[3] != 'U'))
        return {};
    std::string_view body = d.substr(4);
    if (body.size() < 2 || body.substr(body.size() - 2) != "@@")
        return std::string(body);
    body.remove_suffix(2);
    if (body.find_first_of("?$<") != std::string_view::npos)
        return std::string(body); // a template or a nested decoration: kept as written
    std::vector<std::string_view> parts;
    size_t at = 0;
    while (at <= body.size()) {
        const size_t next = body.find('@', at);
        parts.push_back(body.substr(at, next == std::string_view::npos ? std::string_view::npos : next - at));
        if (next == std::string_view::npos)
            break;
        at = next + 1;
    }
    std::string out;
    for (size_t i = parts.size(); i-- > 0;) {
        if (parts[i].empty())
            continue;
        if (!out.empty())
            out += "::";
        out += parts[i];
    }
    return out;
}

namespace {
bool contains(std::string_view hay, std::string_view needle) {
    if (needle.size() > hay.size())
        return false;
    for (size_t i = 0; i + needle.size() <= hay.size(); ++i) {
        bool same = true;
        for (size_t k = 0; k < needle.size() && same; ++k) {
            char a = hay[i + k], b = needle[k];
            if (a >= 'A' && a <= 'Z')
                a = static_cast<char>(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z')
                b = static_cast<char>(b - 'A' + 'a');
            same = a == b;
        }
        if (same)
            return true;
    }
    return false;
}
} // namespace

bool botClass(std::string_view name) {
    // game_targets.cpp: the bots' mover managers and the character bot classes it names.
    for (const char* n : {"BotMoverManager", "HoverMoverManager", "ThugBot", "CivilianBot", "AllyBot",
                          "MissionFollowBot", "SilverSableCraftBot"})
        if (contains(name, n))
            return true;
    return false;
}

bool suitClass(std::string_view name) {
    for (const char* n : {"SkinManager", "SunsetVanityEquippedInventory", "ItemLoadoutComponent", "HeroCharacterManager"}) {
        const std::string_view w(n);
        // the whole class name, or its last part after "::"
        if (name == w || (name.size() > w.size() + 2 && name.substr(name.size() - w.size()) == w &&
                          name.substr(name.size() - w.size() - 2, 2) == "::"))
            return true;
    }
    return false;
}

bool vehicleClass(std::string_view name) {
    return name.find("Vehicle") != std::string_view::npos || name.find("Traffic") != std::string_view::npos ||
           name.find("Driv") != std::string_view::npos;
}

const char* depthFormatName(uint32_t dxgi) {
    switch (dxgi) {
    case 19: return "R32G8X24_TYPELESS";
    case 20: return "D32_FLOAT_S8X24_UINT";
    case 39: return "R32_TYPELESS";
    case 40: return "D32_FLOAT";
    case 44: return "R24G8_TYPELESS";
    case 45: return "D24_UNORM_S8_UINT";
    case 53: return "R16_TYPELESS";
    case 55: return "D16_UNORM";
    default: return "";
    }
}
bool depthFormat(uint32_t dxgi) { return *depthFormatName(dxgi) != 0; }

const char* eyeTargetVerdict(uint32_t dxgi, uint64_t width, uint32_t height, uint32_t renderWidth, uint32_t renderHeight) {
    const bool eyeSize = renderWidth && renderHeight && width == renderWidth && height == renderHeight;
    if (depthFormat(dxgi))
        return eyeSize ? "depthAtEyeSize" : "depthOtherSize";
    return eyeSize ? "atEyeSize" : "other";
}

std::string eyeDepthStatus(const std::vector<EyeDepthTally>& tally, uint32_t passes) {
    std::vector<const EyeDepthTally*> every, some;
    for (const auto& t : tally) {
        if (!t.atEyeSize)
            continue;
        some.push_back(&t);
        if (passes && t.atEyeSize == passes * 2 && !t.shared)
            every.push_back(&t);
    }
    auto named = [](const EyeDepthTally& t) {
        return std::to_string(t.index) + (t.name.empty() ? std::string() : " (" + t.name + ")");
    };
    if (passes >= 2 && every.size() == 1)
        return "CONFIRMED record " + named(*every[0]);
    if (some.empty())
        return "not found";
    std::string s = "CANDIDATE records";
    for (size_t i = 0; i < some.size(); ++i)
        s += (i ? ", " : " ") + named(*some[i]);
    return s;
}

bool interesting(std::string_view name, Area& area) {
    struct Key {
        const char* word;
        Area area;
    };
    static constexpr Key keys[] = {
        // Spawning and seating first (the area with no SpidyVR anchor of its own).
        {"Vehicle", Area::spawning}, {"Car", Area::spawning},     {"Seat", Area::spawning},
        {"Driver", Area::spawning},  {"Driving", Area::spawning}, {"Spawn", Area::spawning},
        {"Traffic", Area::spawning}, {"Prefab", Area::spawning},  {"Passenger", Area::spawning},
        {"Mount", Area::spawning},   {"Rider", Area::spawning},    {"Bike", Area::spawning},
        {"Helicopter", Area::spawning},
        // Held props: what attaches to a joint.
        {"Attach", Area::hands},     {"Socket", Area::hands},     {"Prop", Area::hands},
        {"Throwable", Area::hands},  {"Carry", Area::hands},      {"Held", Area::hands},
        {"Pickup", Area::hands},     {"Joint", Area::hands},
        // Enemies and damage.
        {"Bot", Area::enemies},      {"Enemy", Area::enemies},    {"Health", Area::damage},
        {"Damage", Area::damage},    {"Attack", Area::damage},    {"Status", Area::damage},
        {"Knock", Area::damage},
        // The grapple and the world query.
        {"Rope", Area::grapple},     {"Web", Area::grapple},      {"Zip", Area::grapple},
        {"Anchor", Area::grapple},   {"Swing", Area::grapple},    {"Grapple", Area::grapple},
        {"RayCast", Area::grapple},  {"Raycast", Area::grapple},  {"Query", Area::grapple},
        {"Collector", Area::grapple},
        // The hero.
        {"Hero", Area::player},      {"Mover", Area::player},
        // Physics.
        {"hknp", Area::physics},     {"Physics", Area::physics},  {"Collision", Area::physics},
    };
    for (const Key& k : keys)
        if (contains(name, k.word)) {
            area = k.area;
            return true;
        }
    return false;
}

bool actorTransform(const float m[16]) {
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(m[i]) || std::fabs(m[i]) >= 1e7f)
            return false;
    for (int i = 0; i < 3; ++i) {
        const float* a = m + i * 4;
        if (std::fabs(a[0] * a[0] + a[1] * a[1] + a[2] * a[2] - 1) > .15f)
            return false;
        for (int j = i + 1; j < 3; ++j) {
            const float* b = m + j * 4;
            if (std::fabs(a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) > .1f)
                return false;
        }
    }
    return true;
}

std::string hex(const void* p, size_t n) {
    static const char digits[] = "0123456789abcdef";
    std::string s(n * 2, '0');
    const auto* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; ++i) {
        s[i * 2] = digits[b[i] >> 4];
        s[i * 2 + 1] = digits[b[i] & 15];
    }
    return s;
}

std::string quoted(std::string_view s) {
    std::string o = "\"";
    for (const char c : s) {
        const auto u = static_cast<unsigned char>(c);
        if (c == '"' || c == '\\') {
            o += '\\';
            o += c;
        } else if (u < 0x20 || u >= 0x7f) {
            char b[8];
            std::snprintf(b, sizeof(b), "\\u%04x", u);
            o += b;
        } else {
            o += c;
        }
    }
    return o + "\"";
}

std::string number(double v) {
    if (!std::isfinite(v))
        return "null";
    char b[40];
    std::snprintf(b, sizeof(b), "%.6g", v);
    return b;
}

void findAll(const uint8_t* data, size_t n, uint64_t at, const uint8_t* pattern, size_t patternBytes,
             std::vector<uint64_t>& hits, size_t maxHits) {
    if (!patternBytes || n < patternBytes)
        return;
    const uint8_t first = pattern[0];
    for (size_t i = 0; i + patternBytes <= n && hits.size() < maxHits; ++i) {
        const void* f = std::memchr(data + i, first, n - patternBytes + 1 - i);
        if (!f)
            return;
        i = static_cast<size_t>(static_cast<const uint8_t*>(f) - data);
        if (std::memcmp(data + i, pattern, patternBytes) == 0)
            hits.push_back(at + i);
    }
}

std::string sessionName(int year, int month, int day, int hour, int minute, int second) {
    char b[48];
    std::snprintf(b, sizeof(b), "sm1scan-%04d%02d%02d-%02d%02d%02d", year, month, day, hour, minute, second);
    return b;
}

std::vector<std::string> oldSessions(std::vector<std::string> files, uint32_t keep) {
    std::vector<std::string> sessions;
    auto stem = [](const std::string& f) -> std::string {
        if (f.rfind("sm1scan-", 0) != 0)
            return {};
        const size_t dot = f.find('.');
        if (dot == std::string::npos)
            return {};
        const std::string ext = f.substr(dot);
        if (ext != ".jsonl" && ext != ".bin" && ext != ".txt")
            return {};
        return f.substr(0, dot);
    };
    for (const auto& f : files)
        if (const auto s = stem(f); !s.empty() && std::find(sessions.begin(), sessions.end(), s) == sessions.end())
            sessions.push_back(s);
    std::sort(sessions.begin(), sessions.end()); // the names sort by time
    std::vector<std::string> drop;
    if (sessions.size() <= keep)
        return drop;
    sessions.resize(sessions.size() - keep);
    for (const auto& f : files)
        if (const auto s = stem(f); !s.empty() && std::find(sessions.begin(), sessions.end(), s) != sessions.end())
            drop.push_back(f);
    std::sort(drop.begin(), drop.end());
    return drop;
}
} // namespace spidy::sm1::scan
