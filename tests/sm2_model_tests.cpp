// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// 0.0.76 REMOTE SUIT reader (include/spidy/sm2_model.hpp): LZ4 blocks (hand-made ones, overlaps, long lengths,
// every malformed kind refused), a DSAR block table and a read across two blocks, DAT1 sections, and the
// CANDIDATE .model layouts on tests/data/sm2model/synth.model - the same synthetic model (and its LZ4 block)
// tests/hero076_sm2_model_tests.py checks with tools/sm2_suits/sm2model.py, so the two readers agree.
// Given the folder as argv[1] (CTest does). No game data.
#include "spidy/sm2_model.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace spidy::sm2_model;
namespace {
int failures{};
void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "  PASS" : "  FAIL", what);
    failures += ok ? 0 : 1;
}
std::vector<uint8_t> bytes(std::initializer_list<int> v) {
    std::vector<uint8_t> out;
    for (int x : v)
        out.push_back(static_cast<uint8_t>(x));
    return out;
}
std::vector<uint8_t> file(const std::string& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
std::string text(const std::vector<uint8_t>& v) { return std::string(v.begin(), v.end()); }
bool allOk(const Model& m) {
    for (const auto& n : m.notes)
        if (n.compare(0, 4, "no: ") == 0) {
            std::printf("    %s\n", n.c_str());
            return false;
        }
    return !m.notes.empty();
}
} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::vector<uint8_t> out;
    // "abcd", then 8 bytes from 4 back (an overlapping copy), then the literal "x"
    auto a = bytes({0x44, 'a', 'b', 'c', 'd', 4, 0, 0x10, 'x'});
    check(lz4Block(a.data(), a.size(), 13, out) && text(out) == "abcdabcdabcdx", "LZ4: a match that overlaps itself");
    // one literal, a run of 1 + (15 + 4) + 255 + 6 = 280 copies of it, then "end" (both length extensions)
    auto b = bytes({0x1F, 'z', 1, 0, 255, 6, 0x30, 'e', 'n', 'd'});
    check(lz4Block(b.data(), b.size(), 1 + 15 + 4 + 255 + 6 + 3, out) && out.size() == 284 && out[283] == 'd' &&
              out[1] == 'z' && out[279] == 'z' && out[280] == 'z',
          "LZ4: long match lengths");
    std::vector<uint8_t> lits(300, 'q');
    std::vector<uint8_t> c{0xF0, 255, 30};
    c.insert(c.end(), lits.begin(), lits.end());
    check(lz4Block(c.data(), c.size(), 300, out) && out == lits, "LZ4: long literal lengths");
    auto badOff = bytes({0x10, 'a', 5, 0});
    check(!lz4Block(badOff.data(), badOff.size(), 10, out) && out.empty(), "LZ4: an offset before the start is refused");
    auto zeroOff = bytes({0x10, 'a', 0, 0});
    check(!lz4Block(zeroOff.data(), zeroOff.size(), 10, out), "LZ4: offset 0 is refused");
    auto shortLit = bytes({0x50, 'a', 'b'});
    check(!lz4Block(shortLit.data(), shortLit.size(), 5, out), "LZ4: literals past the input are refused");
    check(!lz4Block(a.data(), a.size(), 12, out) && !lz4Block(a.data(), a.size(), 14, out), "LZ4: the wrong size is refused");
    auto cut = bytes({0xF0, 255});
    check(!lz4Block(cut.data(), cut.size(), 300, out), "LZ4: a length running off the end is refused");
    check(lz4Block(nullptr, 0, 0, out) && out.empty() && !lz4Block(nullptr, 3, 0, out), "LZ4: empty");

    // DSAR: a 2-block table, the asset spanning both (block 1 LZ4 "abcdabcdabcdx", block 2 the long literals)
    std::vector<uint8_t> arch(32 + 64, 0);
    auto put32 = [&](size_t at, uint32_t v) { std::memcpy(arch.data() + at, &v, 4); };
    auto put64 = [&](size_t at, uint64_t v) { std::memcpy(arch.data() + at, &v, 8); };
    put32(0, dsarMagic), put32(12, 96);
    put64(32, 0), put64(40, 96), put32(48, 13), put32(52, static_cast<uint32_t>(a.size())), arch[56] = 3;
    put64(64, 13), put64(72, 96 + a.size()), put32(80, 300), put32(84, static_cast<uint32_t>(c.size())), arch[88] = 3;
    arch.insert(arch.end(), a.begin(), a.end());
    arch.insert(arch.end(), c.begin(), c.end());
    std::vector<Block> blocks;
    check(dsarHeaderBytes(Bytes{arch.data(), 16}) == 96 && dsarBlocks(Bytes{arch.data(), 96}, blocks) && blocks.size() == 2 &&
              blocks[1].raw == 13 && blocks[1].rawSize == 300 && blocks[1].type == 3,
          "DSAR: the block table");
    auto readAt = [&](uint64_t off, uint32_t n, uint8_t* to) {
        if (off > arch.size() || n > arch.size() - off)
            return false;
        std::memcpy(to, arch.data() + off, n);
        return true;
    };
    std::string why;
    check(dsarRead(blocks, readAt, 9, 10, out, why) && text(out) == "bcdxqqqqqq", "DSAR: an asset across two LZ4 blocks");
    blocks[1].type = 2;
    check(!dsarRead(blocks, readAt, 9, 10, out, why) && why.find("GDeflate") != std::string::npos,
          "DSAR: GDeflate is said, not guessed");
    blocks[1].type = 3;
    blocks[1].compSize = 9999;
    check(!dsarRead(blocks, readAt, 9, 10, out, why) && why == "a short read", "DSAR: a short read is said");
    check(!dsarBlocks(Bytes{arch.data(), 40}, blocks), "DSAR: a cut table is refused");

    check(nameHash("root") == 0x865696D5u && nameHash("spine") == 0x4A5D0D0Au, "the name hash (as sm2scan.py name_hash)");

    const std::string dir = argc > 1 ? argv[1] : "tests/data/sm2model";
    const auto model = file(dir + "/synth.model"), packed = file(dir + "/synth.lz4");
    check(!model.empty() && !packed.empty(), "the synthetic model (tests/data/sm2model)");
    if (model.empty() || packed.empty()) { std::printf("FAILED (pass the tests/data/sm2model folder)\n"); return 1; }
    std::vector<uint8_t> unpacked;
    check(lz4Block(packed.data(), packed.size(), model.size(), unpacked) && unpacked == model,
          "its LZ4 block (written by the Python test's compressor) reads back");
    Model m;
    check(m.read(Bytes{model.data(), model.size()}) && allOk(m), "every CANDIDATE check holds on it");
    check(m.joints.size() == 3 && m.joints[0].name == "root" && m.joints[2].name == "head" && m.joints[2].parent == 1,
          "the skeleton");
    check(m.bind.size() == 36 && m.bind[3] == 0.f && m.bind[12 + 3] == 1.f, "the joint transforms");
    check(m.meshes.size() == 2 && m.meshes[1].vertexStart == 3 && m.meshes[1].indexCount == 3 && m.meshes[1].material == 1 &&
              (m.meshes[0].flags & 0x10),
          "the meshes");
    check(m.hasBuilt && m.built.vertexCount == 6 && m.built.indexCount == 6 && std::fabs(m.built.scale * 4096.f - 1.f) < 1e-6f,
          "the built block");
    const auto v = m.vertexes();
    check(v.size() == 6 && std::fabs(v[2].pos[0] - 2.f) < 1e-5f && std::fabs(v[2].pos[1] - 0.f) < 1e-5f &&
              std::fabs(v[2].pos[2] - 0.5f) < 1e-5f && std::fabs(v[2].uv[0] - 2048.f / 1024.f) < 1e-5f,
          "the vertexes (if the guess holds)");
    check(m.indexes16().size() == 6, "the indexes (if the guess holds)");
    // A wrong guess: the counts no longer fit, and nothing is decoded from it.
    std::vector<uint8_t> wrong = model;
    for (const auto& s : m.dat1.sections)
        if (s.tag == tag::built) {
            const uint32_t twelve = 12;
            std::memcpy(wrong.data() + s.off + 0x68, &twelve, 4);
        }
    Model w;
    check(w.read(Bytes{wrong.data(), wrong.size()}) && !allOk(w) && w.vertexes().empty(), "a wrong guess says no, decodes nothing");
    std::vector<uint8_t> junk(64, 7);
    Model j;
    check(!j.read(Bytes{junk.data(), junk.size()}) && j.notes.size() == 1, "not a DAT1");
    Model cutModel;
    check(!cutModel.read(Bytes{model.data(), 40}), "a cut model is refused");
    std::printf("%s\n", failures ? "FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
