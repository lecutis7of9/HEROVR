// Update 0.0.72 GAME BODY (EXPERIMENTAL): game_body.hpp.
#include "spidy/game_body.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace spidy::game_body {
uint32_t nameHash(const char* name) {
    static const auto table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = c & 1 ? (c >> 1) ^ 0xedb88320u : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    uint32_t c = 0xedb88320u;
    for (; *name; ++name)
        c = table[(c ^ static_cast<uint8_t>(*name)) & 0xff] ^ (c >> 8);
    return c;
}

bool parseJoints(std::span<const uint8_t> bytes, uint32_t count, Table& out) {
    out = {};
    if (!count || count > maxJoints || bytes.size() < size_t(count) * jointBytes)
        return false;
    out.parent.resize(count);
    out.hash.resize(count);
    for (uint32_t j = 0; j < count; ++j) {
        std::memcpy(&out.parent[j], &bytes[j * jointBytes], 2);
        std::memcpy(&out.hash[j], &bytes[j * jointBytes + 8], 4);
        if (out.parent[j] >= static_cast<int>(j) || out.parent[j] < -1) {
            out = {};
            return false;
        }
    }
    return true;
}

bool restToModel(std::span<const uint8_t> rest, const Table& table, std::vector<float>& out) {
    const size_t n = table.parent.size();
    if (!n || rest.size() < n * restBytes)
        return false;
    out.assign(n * 16, 0.f);
    for (size_t j = 0; j < n; ++j) {
        float e[12];
        std::memcpy(e, &rest[j * restBytes], sizeof(e));
        const Vec3 s{e[0], e[1], e[2]};
        const Quat q = body::unit({e[4], e[5], e[6], e[7]});
        const Vec3 t{e[8], e[9], e[10]};
        Vec3 axes[3] = {q.rotate({s.x, 0, 0}), q.rotate({0, s.y, 0}), q.rotate({0, 0, s.z})};
        Vec3 at = t;
        if (const int p = table.parent[j]; p >= 0) {
            const float* m = &out[p * 16];
            auto send = [&](Vec3 v, bool point) {
                Vec3 r{m[0] * v.x + m[4] * v.y + m[8] * v.z, m[1] * v.x + m[5] * v.y + m[9] * v.z,
                       m[2] * v.x + m[6] * v.y + m[10] * v.z};
                return point ? r + Vec3{m[12], m[13], m[14]} : r;
            };
            for (auto& a : axes)
                a = send(a, false);
            at = send(at, true);
        }
        float* m = &out[j * 16];
        for (int r = 0; r < 3; ++r) {
            m[r * 4] = axes[r].x;
            m[r * 4 + 1] = axes[r].y;
            m[r * 4 + 2] = axes[r].z;
            m[r * 4 + 3] = 0;
        }
        m[12] = at.x;
        m[13] = at.y;
        m[14] = at.z;
        m[15] = 1;
        for (int k = 0; k < 16; ++k)
            if (!std::isfinite(m[k]))
                return false;
    }
    return true;
}

int byName(const Table& t, const char* name) {
    const auto h = nameHash(name);
    for (size_t j = 0; j < t.hash.size(); ++j)
        if (t.hash[j] == h)
            return static_cast<int>(j);
    return -1;
}

std::vector<int16_t> chain(const Table& t, int from, int to) {
    std::vector<int16_t> out;
    for (int j = to; j >= 0 && j != from; j = t.parent[j]) {
        out.push_back(static_cast<int16_t>(j));
        if (out.size() > t.parent.size())
            return {};
        if (t.parent[j] < 0)
            return {};
    }
    if (to < 0)
        return {};
    std::reverse(out.begin(), out.end());
    return out;
}

// The hero's rig (Spider-Man Remastered 4.0630 and Spider-Man 2, every suit
// tried so far; tools/probe_game_body.py lists it): a_body is the root of
// the body, with the pelvis (and under it the legs) and the spine as its
// children. Fingers A to D run index to little finger; B_B is the middle
// knuckle. Moved from native_body.cpp unchanged (October 27).
bool identify(const Table& data, body::Rig& rig, std::vector<std::string>* missing) {
    rig = {};
    rig.parent = data.parent;
    std::vector<std::string> lost;
    auto need = [&](int j, const std::string& name) {
        if (j < 0)
            lost.push_back(name);
        return j;
    };
    rig.pelvis = byName(data, "a_body");
    if (rig.pelvis < 0)
        rig.pelvis = byName(data, "pelvis");
    if (rig.pelvis < 0)
        lost.push_back("a_body/pelvis");
    rig.head = need(byName(data, "head"), "head");
    const int chest = need(byName(data, "chest"), "chest");
    const char* side[2] = {"LF_", "RT_"};
    for (int i = 0; i < 2; ++i) {
        const std::string s = side[i];
        auto named = [&](const std::string& name) { return byName(data, (s + name).c_str()); };
        auto required = [&](const std::string& name) { return need(named(name), s + name); };
        rig.arms[i] = {named("clavicle"),          required("uparm"),     required("loarm"), required("wrist"),
                       required("finger_B_B"), named("thumb_A"), {}, {}};
        rig.legs[i] = {required("upleg"), required("loleg"), required("foot")};
        rig.eyes[i] = named("middle_eye_deform");
        for (const char* finger : {"A", "B", "C", "D"}) {
            std::vector<int16_t> joints;
            for (const char* joint : {"_A", "_B", "_C", "_D", "_D_end"})
                if (const int j = named(std::string("finger_") + finger + joint); j >= 0)
                    joints.push_back(static_cast<int16_t>(j));
            if (joints.size() == 5)
                rig.arms[i].fingers.push_back(joints);
        }
        for (const char* joint : {"thumb_A", "thumb_B", "thumb_C", "thumb_C_end"})
            if (const int j = named(joint); j >= 0)
                rig.arms[i].thumbChain.push_back(static_cast<int16_t>(j));
        if (rig.arms[i].thumbChain.size() != 4)
            rig.arms[i].thumbChain.clear();
    }
    if (rig.pelvis >= 0 && chest >= 0) {
        rig.spine = chain(data, rig.pelvis, chest);
        if (rig.spine.empty())
            lost.push_back("spine");
    }
    if (rig.head >= 0 && chest >= 0) {
        rig.neck = chain(data, chest, data.parent[rig.head]);
        if (data.parent[rig.head] != chest && rig.neck.empty())
            lost.push_back("neck");
    }
    if (missing)
        *missing = lost;
    return lost.empty();
}

namespace {
bool under(const Table& t, int j, int top) {
    for (int k = j, steps = 0; k >= 0 && steps <= static_cast<int>(t.parent.size()); k = t.parent[k], ++steps)
        if (k == top)
            return true;
    return false;
}
int rootOf(const Table& t, int j) {
    int k = j;
    for (int steps = 0; k >= 0 && t.parent[k] >= 0 && steps <= static_cast<int>(t.parent.size()); ++steps)
        k = t.parent[k];
    return k;
}
} // namespace

bool protectedClear(const Table& t, const body::Rig& rig, std::string* failed) {
    std::vector<int> moved{rig.pelvis, rig.head};
    for (const auto& a : rig.arms)
        moved.insert(moved.end(), {a.clavicle, a.upper});
    for (const auto& l : rig.legs)
        moved.push_back(l.upper);
    auto hit = [&](int j) {
        for (const int m : moved)
            if (m >= 0 && under(t, j, m))
                return true;
        return false;
    };
    for (const char* name : protectedNames)
        if (const int j = byName(t, name); j >= 0 && hit(j)) {
            if (failed)
                *failed = name;
            return false;
        }
    const int bodyRoot = rig.pelvis >= 0 ? rootOf(t, rig.pelvis) : -1;
    for (size_t j = 0; j < t.parent.size(); ++j)
        if (t.parent[j] < 0 && static_cast<int>(j) != bodyRoot && hit(static_cast<int>(j))) {
            if (failed)
                *failed = "root " + std::to_string(j);
            return false;
        }
    return true;
}

bool Gate::feed(uintptr_t playerRecord, const char* failedCheck) {
    if (failedCheck || !playerRecord) {
        failed = failedCheck ? failedCheck : "no player";
        passes = 0;
        verified = false;
        record = playerRecord;
        return false;
    }
    if (playerRecord != record) {
        record = playerRecord;
        passes = 0;
        verified = false;
    }
    failed.clear();
    if (passes < needed)
        ++passes;
    verified = passes >= needed;
    return verified;
}

std::vector<Section> sections(std::span<const uint8_t> h) {
    std::vector<Section> out;
    auto u16 = [&](size_t at) -> uint32_t {
        if (at + 2 > h.size())
            return 0;
        uint16_t v;
        std::memcpy(&v, &h[at], 2);
        return v;
    };
    auto u32 = [&](size_t at) -> uint32_t {
        if (at + 4 > h.size())
            return 0;
        uint32_t v;
        std::memcpy(&v, &h[at], 4);
        return v;
    };
    if (u16(0) != 0x5a4d)
        return out;
    const uint32_t pe = u32(0x3c);
    if (pe > h.size() || u32(pe) != 0x4550)
        return out;
    const uint32_t count = u16(pe + 6), optional = u16(pe + 20);
    const size_t first = pe + 24 + optional;
    for (uint32_t i = 0; i < count && i < 96; ++i) {
        const size_t s = first + i * 40;
        if (s + 40 > h.size())
            return {};
        Section sec;
        sec.size = u32(s + 8);
        sec.rva = u32(s + 12);
        sec.code = (u32(s + 36) & 0x20000020u) != 0; // code, or executable
        out.push_back(sec);
    }
    return out;
}

bool matches(std::span<const uint8_t> image, uintptr_t rva, std::span<const uint8_t> bytes) {
    return rva + bytes.size() <= image.size() && !std::memcmp(&image[rva], bytes.data(), bytes.size());
}

std::vector<uintptr_t> findAll(std::span<const uint8_t> image, std::span<const Section> where,
                               std::span<const uint8_t> bytes, size_t limit) {
    std::vector<uintptr_t> out;
    if (bytes.empty())
        return out;
    for (const auto& s : where) {
        const size_t end = std::min<size_t>(image.size(), size_t(s.rva) + s.size);
        if (s.rva >= end || end - s.rva < bytes.size())
            continue;
        const uint8_t* from = image.data() + s.rva;
        const uint8_t* last = image.data() + end - bytes.size() + 1;
        for (auto* p = from; p < last;) {
            p = static_cast<const uint8_t*>(std::memchr(p, bytes[0], last - p));
            if (!p)
                break;
            if (!std::memcmp(p, bytes.data(), bytes.size())) {
                out.push_back(p - image.data());
                if (out.size() >= limit)
                    return out;
            }
            ++p;
        }
    }
    return out;
}

bool calls(std::span<const uint8_t> image, uintptr_t from, uintptr_t to, size_t length) {
    for (size_t i = 0; i + 5 <= length && from + i + 5 <= image.size(); ++i) {
        const uint8_t op = image[from + i];
        if (op != 0xE8 && op != 0xE9)
            continue;
        int32_t rel;
        std::memcpy(&rel, &image[from + i + 1], 4);
        if (static_cast<intptr_t>(from + i + 5) + rel == static_cast<intptr_t>(to))
            return true;
    }
    return false;
}

uintptr_t vtableByRtti(std::span<const uint8_t> image, uint64_t imageBase, const char* mangled) {
    const std::span<const uint8_t> name(reinterpret_cast<const uint8_t*>(mangled), std::strlen(mangled) + 1);
    const Section all{0, static_cast<uint32_t>(image.size()), false};
    const auto names = findAll(image, std::span(&all, 1), name, 4);
    if (names.size() != 1 || names[0] < 0x10)
        return 0;
    const uint32_t td = static_cast<uint32_t>(names[0] - 0x10);
    // Complete-object locators: signature 1, offset 0, cd offset, type
    // descriptor RVA, hierarchy RVA, self RVA (x64).
    uintptr_t found = 0;
    int count = 0;
    for (size_t at = 0; at + 24 <= image.size(); at += 4) {
        uint32_t f[6];
        std::memcpy(f, &image[at], 4);
        if (f[0] != 1)
            continue;
        std::memcpy(f, &image[at], 24);
        if (f[1] != 0 || f[3] != td || f[5] != at)
            continue;
        const uint64_t pointer = imageBase + at;
        const auto refs = findAll(image, std::span(&all, 1),
                                  std::span(reinterpret_cast<const uint8_t*>(&pointer), 8), 4);
        for (const auto r : refs)
            if (r % 8 == 0) {
                found = r + 8;
                ++count;
            }
    }
    return count == 1 ? found : 0;
}

bool rttiNamed(const Reader& read, uint64_t imageBase, uintptr_t vtableRva, const char* prefix) {
    uint64_t col{};
    if (!vtableRva || vtableRva < 8 || !read(vtableRva - 8, &col, 8) || col <= imageBase)
        return false;
    const uint64_t colRva = col - imageBase;
    uint32_t f[6]{};
    if (colRva > 0xffffffffu || !read(colRva, f, sizeof(f)) || f[0] != 1 || f[1] != 0 || f[5] != colRva)
        return false;
    const size_t n = std::strlen(prefix);
    std::string name(n, '\0');
    return n && n < 256 && read(uintptr_t(f[3]) + 0x10, name.data(), n) && name == prefix;
}

bool findFunctions(std::span<const uint8_t> image, std::span<const Section> code, Functions& out) {
    out = {};
    if (matches(image, candidateWriter, writerPrologue) && matches(image, candidateToModel, toModelPrologue) &&
        calls(image, candidateWriter, candidateToModel)) {
        out = {candidateWriter, candidateToModel, false};
        return true;
    }
    const auto writers = findAll(image, code, writerPrologue, 4096);
    const auto converters = findAll(image, code, toModelPrologue, 4096);
    int pairs = 0;
    for (const auto w : writers)
        for (const auto c : converters)
            if (calls(image, w, c)) {
                out = {w, c, true};
                ++pairs;
            }
    if (pairs != 1)
        out = {};
    return pairs == 1;
}
bool candidatesHold(const Reader& read) {
    std::array<uint8_t, 0x400> code{};
    std::array<uint8_t, toModelPrologue.size()> head{};
    if (!read(candidateWriter, code.data(), code.size()) || !read(candidateToModel, head.data(), head.size()))
        return false;
    if (std::memcmp(code.data(), writerPrologue.data(), writerPrologue.size()) ||
        std::memcmp(head.data(), toModelPrologue.data(), head.size()))
        return false;
    for (size_t i = 0; i + 5 <= code.size(); ++i) {
        if (code[i] != 0xE8 && code[i] != 0xE9)
            continue;
        int32_t rel;
        std::memcpy(&rel, &code[i + 1], 4);
        if (static_cast<intptr_t>(candidateWriter + i + 5) + rel == static_cast<intptr_t>(candidateToModel))
            return true;
    }
    return false;
}
} // namespace spidy::game_body
