// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// 0.0.76 REMOTE SUIT, the reading half: PLAYER 2 drawn in the suit his friend wears, from this PC's own installed
// game files (nothing of the game's is ever shipped or sent; the friend's suit travels as one id, the MP 'S' tail
// in include/spidy/mp_net.hpp). Pure: no game memory, no files - the caller hands in bytes it read itself from
// the user's toc and archives (tools/sm2_suits/sm2model.py does the same in Python, for checking on his PC).
//
// What is known, and how well:
//  CONFIRMED (sm2scan.py read the user's real game with exactly this): the toc (0x34E89035, then a DAT1 with the
//    asset ids, sizes and archives sections), the DSAR archive block table (32-byte entries, type 3 = LZ4 block,
//    type 2 = GDeflate), the LZ4 block format (228 of the 230 suit models; 2 stored raw), and every suit .model
//    being a DAT1 of type 0x1B069A8A whose sections all have the tags listed under tag:: below.
//  CANDIDATE (from ALERT's dat1lib for Ratchet & Clank: Rift Apart, the same engine, GPL-3, see
//    THIRD_PARTY_NOTICES; not yet checked against a Spider-Man 2 .model): the layouts inside those sections -
//    joints, joint transforms, the 120-byte "built" block, the 64-byte mesh entries - and above all which
//    sections hold the vertexes and indexes (Rift Apart's tags A98BE69B / 0859863D are absent from every SM2 suit
//    model; 2C2CA205 and F4EA9E18 are the guesses, checked below against the built counts).
// Nothing here is used to draw yet: first sm2model.py's report from the user's PC must show the checks hold.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace spidy::sm2_model {

constexpr uint32_t tocMagic = 0x34E89035, dsarMagic = 0x52415344, dat1Magic = 0x44415431;
constexpr uint32_t modelType = 0x1B069A8A; // CONFIRMED: all 230 suit models in the user's scan

namespace tag {
constexpr uint32_t assetIds = 0x506D7B8A, sizes = 0x65BCF461, archives = 0x398ABFF0; // the toc (CONFIRMED)
constexpr uint32_t joints = 0x15DF9D3B;          // CANDIDATE layout: 16 bytes a joint
constexpr uint32_t jointTransforms = 0xDCC88A19; // CANDIDATE: count 3x4, then (64-aligned) count 4x4 floats
constexpr uint32_t built = 0x283D0383;           // CANDIDATE: 120 bytes; scale, offset, counts
constexpr uint32_t meshes = 0x78D9CBDE;          // CANDIDATE: 64 bytes a mesh
constexpr uint32_t materials = 0x3250BB80;
constexpr uint32_t vertexes = 0x2C2CA205; // CANDIDATE guess (an unverified web summary), checked by size
constexpr uint32_t indexes = 0xF4EA9E18;  // CANDIDATE guess, checked by size
} // namespace tag

// Little-endian reads that never run past the end.
struct Bytes {
    const uint8_t* p{};
    size_t n{};
    bool has(size_t off, size_t len) const { return off <= n && len <= n - off; }
    template <class T> bool get(size_t off, T& v) const {
        if (!has(off, sizeof(T)))
            return false;
        std::memcpy(&v, p + off, sizeof(T));
        return true;
    }
    template <class T> T at(size_t off) const {
        T v{};
        get(off, v);
        return v;
    }
    Bytes sub(size_t off, size_t len) const { return has(off, len) ? Bytes{p + off, len} : Bytes{}; }
};

// A standard LZ4 block (no frame), as DSAR type 3 holds one: `want` bytes out, exactly. False (out cleared) on
// anything malformed - a length past the input, an offset of 0 or before the start, more than `want`.
inline bool lz4Block(const uint8_t* src, size_t n, size_t want, std::vector<uint8_t>& out) {
    out.clear();
    if (!src && n)
        return false;
    out.reserve(want);
    size_t i = 0;
    auto more = [&](size_t& len) {
        for (;;) {
            if (i >= n)
                return false;
            const uint8_t b = src[i++];
            len += b;
            if (b != 255)
                return true;
        }
    };
    while (i < n) {
        const uint8_t token = src[i++];
        size_t lit = token >> 4;
        if (lit == 15 && !more(lit))
            return out.clear(), false;
        if (lit > n - i || lit > want - out.size())
            return out.clear(), false;
        out.insert(out.end(), src + i, src + i + lit);
        i += lit;
        if (i >= n || out.size() >= want)
            break; // the last sequence is literals only
        if (n - i < 2)
            return out.clear(), false;
        const size_t off = static_cast<size_t>(src[i] | (src[i + 1] << 8));
        i += 2;
        if (off == 0 || off > out.size())
            return out.clear(), false;
        size_t ml = static_cast<size_t>(token & 15) + 4;
        if ((token & 15) == 15 && !more(ml))
            return out.clear(), false;
        if (ml > want - out.size())
            return out.clear(), false;
        const size_t start = out.size() - off;
        for (size_t k = 0; k < ml; ++k) // overlapping copies repeat, as LZ4 means them to
            out.push_back(out[start + k]);
    }
    if (out.size() != want)
        return out.clear(), false;
    return true;
}

// A DSAR archive's blocks (CONFIRMED in the user's scan). `head` is the file's first bytes, at least
// headerBytes(first 16) long.
struct Block {
    uint64_t raw{}, comp{};
    uint32_t rawSize{}, compSize{};
    uint8_t type{}; // 3 = LZ4, 2 = GDeflate (no decoder here yet)
};
inline size_t dsarHeaderBytes(Bytes first) {
    return first.at<uint32_t>(0) == dsarMagic && first.has(12, 4) ? first.at<uint32_t>(12) : 0;
}
inline bool dsarBlocks(Bytes head, std::vector<Block>& out) {
    out.clear();
    const size_t end = dsarHeaderBytes(head);
    if (end < 32 || end > head.n)
        return false;
    for (size_t at = 32; at + 32 <= end; at += 32) {
        Block b;
        b.raw = head.at<uint64_t>(at);
        b.comp = head.at<uint64_t>(at + 8);
        b.rawSize = head.at<uint32_t>(at + 16);
        b.compSize = head.at<uint32_t>(at + 20);
        b.type = head.at<uint8_t>(at + 24);
        out.push_back(b);
    }
    return true;
}
// The bytes [offset, offset + size) of the archive's uncompressed space, through `readAt(fileOffset, bytes, out)`
// (the caller's file read). `why` says what stopped it (a GDeflate block, a short read, bad LZ4).
template <class ReadAt>
bool dsarRead(const std::vector<Block>& blocks, ReadAt&& readAt, uint64_t offset, uint32_t size, std::vector<uint8_t>& out,
              std::string& why) {
    out.clear();
    const uint64_t a = offset, b = offset + size;
    std::vector<uint8_t> comp, plain;
    for (const Block& k : blocks) {
        if (k.raw + k.rawSize <= a || k.raw >= b)
            continue;
        if (k.type != 3) {
            why = k.type == 2 ? "a GDeflate block (no decoder yet)" : "compression type " + std::to_string(k.type);
            return false;
        }
        comp.resize(k.compSize);
        if (!readAt(k.comp, k.compSize, comp.data())) {
            why = "a short read";
            return false;
        }
        if (!lz4Block(comp.data(), comp.size(), k.rawSize, plain)) {
            why = "bad LZ4";
            return false;
        }
        const uint64_t from = std::max(k.raw, a) - k.raw, to = std::min(b, k.raw + k.rawSize) - k.raw;
        out.insert(out.end(), plain.begin() + static_cast<std::ptrdiff_t>(from), plain.begin() + static_cast<std::ptrdiff_t>(to));
    }
    if (out.size() != size) {
        why = "the blocks do not cover the asset";
        return false;
    }
    return true;
}

// A DAT1: its type, sections (tag, offset, size; offsets from the DAT1's first byte) and strings.
struct Dat1 {
    struct Section {
        uint32_t tag{}, off{}, size{};
    };
    Bytes data;
    uint32_t type{}, size{};
    std::vector<Section> sections;
    size_t stringsStart{}, stringsEnd{};
    bool parse(Bytes d) {
        *this = Dat1{};
        if (d.at<uint32_t>(0) != dat1Magic || !d.has(0, 16))
            return false;
        data = d;
        type = d.at<uint32_t>(4);
        size = d.at<uint32_t>(8);
        const uint16_t count = d.at<uint16_t>(12), unknowns = d.at<uint16_t>(14);
        if (!d.has(16, size_t{12} * count))
            return false;
        size_t first = d.n;
        for (uint16_t k = 0; k < count; ++k) {
            Section s{d.at<uint32_t>(16 + 12u * k), d.at<uint32_t>(20 + 12u * k), d.at<uint32_t>(24 + 12u * k)};
            if (!d.has(s.off, s.size))
                return false;
            sections.push_back(s);
            first = std::min<size_t>(first, s.off);
        }
        stringsStart = 16 + 12u * count + 8u * unknowns;
        stringsEnd = first;
        return true;
    }
    Bytes section(uint32_t t) const {
        for (const auto& s : sections)
            if (s.tag == t)
                return data.sub(s.off, s.size);
        return {};
    }
    bool has(uint32_t t) const {
        for (const auto& s : sections)
            if (s.tag == t)
                return true;
        return false;
    }
    std::string string(uint32_t off) const {
        if (off < stringsStart || off >= stringsEnd)
            return {};
        std::string s;
        for (size_t i = off; i < stringsEnd && data.p[i]; ++i)
            s.push_back(static_cast<char>(data.p[i]));
        return s;
    }
};

// The engine's name hash: CRC-32 (0xEDB88320 table) seeded 0xEDB88320, no final xor, the bytes as they are
// (ALERT crc32.hash(name, False); sm2scan.py name_hash).
inline uint32_t nameHash(const std::string& s) {
    static const auto table = [] {
        std::vector<uint32_t> t(256);
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    uint32_t c = 0xEDB88320u;
    for (unsigned char b : s)
        c = table[(c ^ b) & 0xff] ^ (c >> 8);
    return c;
}

struct Joint {
    int16_t parent{-1};
    uint16_t index{};
    uint32_t hash{};
    std::string name;
};
struct Mesh {
    uint64_t id{};
    uint32_t vertexStart{}, indexStart{}, indexCount{}, vertexCount{};
    uint16_t flags{}, material{}, firstSkinBatch{}, skinBatches{};
};
struct Built {
    float offset[3]{}, scale{};
    uint32_t uvShift{}, indexCount{}, vertexCount{};
};
struct Vertex {
    float pos[3]{}, uv[2]{};
};

// One .model read as far as the CANDIDATE layouts allow, and what holds and what does not.
struct Model {
    Dat1 dat1;
    std::vector<Joint> joints;
    std::vector<float> bind; // jointTransforms: count 3x4 floats (the 4x4s after them not kept)
    std::vector<Mesh> meshes;
    Built built;
    bool hasBuilt{};
    std::vector<std::string> notes; // every check, "ok ..." or "no ..."

    bool read(Bytes d) {
        *this = Model{};
        if (!dat1.parse(d)) {
            notes.push_back("no: not a DAT1");
            return false;
        }
        if (dat1.type != modelType) {
            notes.push_back("no: DAT1 type is not 1B069A8A (not a Spider-Man 2 model)");
            return false;
        }
        readJoints();
        readMeshes();
        readBuilt();
        checkGeometry();
        return true;
    }
    // The geometry, IF the CANDIDATE guess holds (vertexes: 16 bytes each, int16 x/y/z then a packed normal then
    // int16 u/v, as Rift Apart's; scale and offset from the built block). Empty when the check failed.
    std::vector<Vertex> vertexes() const {
        std::vector<Vertex> out;
        const Bytes v = dat1.section(tag::vertexes);
        if (!hasBuilt || !strideOk(v.n, built.vertexCount, 16))
            return out;
        const float uvScale = static_cast<float>(1u << (built.uvShift & 0xF)) / 16384.f;
        for (uint32_t i = 0; i < built.vertexCount; ++i) {
            const size_t at = size_t{16} * i;
            Vertex x;
            for (int k = 0; k < 3; ++k)
                x.pos[k] = static_cast<float>(v.at<int16_t>(at + 2u * static_cast<unsigned>(k))) * built.scale + built.offset[k];
            x.uv[0] = static_cast<float>(v.at<int16_t>(at + 12)) * uvScale;
            x.uv[1] = static_cast<float>(v.at<int16_t>(at + 14)) * uvScale;
            out.push_back(x);
        }
        return out;
    }
    std::vector<uint16_t> indexes16() const {
        std::vector<uint16_t> out;
        const Bytes ix = dat1.section(tag::indexes);
        if (!hasBuilt || !strideOk(ix.n, built.indexCount, 2))
            return out;
        out.resize(built.indexCount);
        std::memcpy(out.data(), ix.p, out.size() * 2);
        return out;
    }
    static bool strideOk(size_t bytes, uint32_t count, size_t stride) {
        return count && bytes >= size_t{count} * stride && bytes < size_t{count} * stride + 64; // padded to 64 at most
    }

private:
    void note(bool ok, const std::string& s) { notes.push_back((ok ? "ok: " : "no: ") + s); }
    void readJoints() {
        const Bytes j = dat1.section(tag::joints);
        if (!j.n || j.n % 16) {
            note(false, "joints section missing or not 16-byte entries (" + std::to_string(j.n) + " bytes)");
            return;
        }
        size_t parentsOk = 0, hashesOk = 0, named = 0;
        for (size_t i = 0; i < j.n / 16; ++i) {
            Joint x;
            x.parent = j.at<int16_t>(16 * i);
            x.index = j.at<uint16_t>(16 * i + 2);
            x.hash = j.at<uint32_t>(16 * i + 8);
            x.name = dat1.string(j.at<uint32_t>(16 * i + 12));
            parentsOk += (x.parent == -1 || (x.parent >= 0 && static_cast<size_t>(x.parent) < i)) && x.index == i ? 1 : 0;
            named += x.name.empty() ? 0 : 1;
            hashesOk += !x.name.empty() && nameHash(x.name) == x.hash ? 1 : 0;
            joints.push_back(std::move(x));
        }
        const size_t n = joints.size();
        note(parentsOk == n, std::to_string(parentsOk) + " of " + std::to_string(n) + " joints have index = place and a parent before them");
        note(named == n && hashesOk == n, std::to_string(hashesOk) + " of " + std::to_string(n) + " joint names hash to their CRC-32");
        const Bytes t = dat1.section(tag::jointTransforms);
        const size_t count = t.n / (48 + 64);
        note(count == n, "joint transforms for " + std::to_string(count) + " joints (" + std::to_string(t.n) + " bytes)");
        if (count == n)
            for (size_t i = 0; i < count * 12; ++i)
                bind.push_back(t.at<float>(4 * i));
    }
    void readMeshes() {
        const Bytes m = dat1.section(tag::meshes);
        if (!m.n || m.n % 64) {
            note(false, "meshes section missing or not 64-byte entries (" + std::to_string(m.n) + " bytes)");
            return;
        }
        for (size_t i = 0; i < m.n / 64; ++i) {
            const size_t at = 64 * i;
            Mesh x;
            x.id = m.at<uint64_t>(at + 4);
            x.vertexStart = m.at<uint32_t>(at + 20);
            x.indexStart = m.at<uint32_t>(at + 24);
            x.indexCount = m.at<uint32_t>(at + 28);
            x.vertexCount = m.at<uint32_t>(at + 32);
            x.flags = m.at<uint16_t>(at + 36);
            x.material = m.at<uint16_t>(at + 38);
            x.firstSkinBatch = m.at<uint16_t>(at + 40);
            x.skinBatches = m.at<uint16_t>(at + 42);
            meshes.push_back(x);
        }
    }
    void readBuilt() {
        const Bytes b = dat1.section(tag::built);
        if (b.n < 120) {
            note(false, "built section missing or short (" + std::to_string(b.n) + " bytes)");
            return;
        }
        hasBuilt = true;
        built.offset[0] = b.at<float>(0x1C), built.offset[1] = b.at<float>(0x20), built.offset[2] = b.at<float>(0x24);
        built.scale = b.at<float>(0x2C);
        built.uvShift = b.at<uint32_t>(0x30);
        built.indexCount = b.at<uint32_t>(0x64);
        built.vertexCount = b.at<uint32_t>(0x68);
        const bool sane = built.scale > 0.f && built.scale < 1.f && built.vertexCount && built.indexCount &&
                          built.indexCount % 3 == 0;
        note(sane, "built: scale " + std::to_string(built.scale) + ", " + std::to_string(built.vertexCount) + " vertexes, " +
                       std::to_string(built.indexCount) + " indexes");
    }
    void checkGeometry() {
        if (!hasBuilt)
            return;
        uint64_t maxIndexEnd = 0, maxVertexEnd = 0;
        for (const Mesh& x : meshes) {
            maxIndexEnd = std::max<uint64_t>(maxIndexEnd, uint64_t{x.indexStart} + x.indexCount);
            maxVertexEnd = std::max<uint64_t>(maxVertexEnd, uint64_t{x.vertexStart} + x.vertexCount);
        }
        note(!meshes.empty() && maxIndexEnd <= built.indexCount && maxVertexEnd <= built.vertexCount,
             std::to_string(meshes.size()) + " meshes inside the built counts (indexes to " + std::to_string(maxIndexEnd) +
                 ", vertexes to " + std::to_string(maxVertexEnd) + ")");
        const Bytes v = dat1.section(tag::vertexes), ix = dat1.section(tag::indexes);
        note(strideOk(v.n, built.vertexCount, 16), "2C2CA205 as 16-byte vertexes (" + std::to_string(v.n) + " bytes for " +
                                                       std::to_string(built.vertexCount) + ")");
        note(strideOk(ix.n, built.indexCount, 2), "F4EA9E18 as 16-bit indexes (" + std::to_string(ix.n) + " bytes for " +
                                                     std::to_string(built.indexCount) + ")");
        const std::vector<uint16_t> ids = indexes16();
        if (!ids.empty()) {
            size_t inside = 0, total = 0;
            for (const Mesh& x : meshes)
                for (uint32_t k = 0; k < x.indexCount; ++k) {
                    ++total;
                    if (size_t{x.indexStart} + k >= ids.size())
                        continue;
                    const uint32_t vtx = (x.flags & 0x10) ? x.vertexStart + ids[x.indexStart + k] : ids[x.indexStart + k];
                    inside += vtx < built.vertexCount ? 1 : 0;
                }
            note(total && inside == total,
                 std::to_string(inside) + " of " + std::to_string(total) + " mesh indexes name a vertex that exists");
        }
    }
};

} // namespace spidy::sm2_model
