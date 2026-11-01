// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// 0.0.76 SUIT PROBE (live.txt suit_probe, export SpidySuitProbe 5555; the wrist HUD is full). READ-ONLY, the
// first step of the remote suit: PLAYER 2 is drawn from each PC's own installed game files in the suit his
// friend wears, so the sender has to learn which .model asset its Spider-Man wears. CONFIRMED: the suit list
// (system_progression.config) names each suit's .model paths (tools/sm2_suits/sm2scan.py reads them from the
// user's own files); the instance word +0x88/+0x8c is a render flag set, not a suit id. CANDIDATE: a pointer
// near the player record (the actor) or its instance leads to the worn model's path (a char*, or an asset
// whose path pointer is a word of it). The probe walks those words through the caller's peek (SEH on
// Windows), keeps every "....model" path it finds and logs the set to the console when it changes (a suit
// change). It never writes game memory, calls a game function or hooks anything; off (the default) reads
// nothing. The asset id is the game's: CRC-64 (reflected 0xC96C5795D7870F42) of the normalized path, >>2,
// top bit set (sm2scan.py and Overstrike agree).
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace spidy::suit_probe {

// The live.txt subtext (one sentence, its restart rule).
constexpr const char* help = "SUIT PROBE reads (never writes) the game's memory near your Spider-Man and logs the "
                             ".model files of the suit he wears to the console when you change suit, for the "
                             "remote-suit test; live, no restart";

constexpr std::array<uint64_t, 256> crcTable() {
    std::array<uint64_t, 256> t{};
    for (uint64_t i = 0; i < 256; ++i) {
        uint64_t c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (c >> 1) ^ 0xC96C5795D7870F42ull : c >> 1;
        t[i] = c;
    }
    return t;
}
inline constexpr auto table = crcTable();

inline std::string normalize(std::string_view path) {
    std::string out;
    out.reserve(path.size());
    for (char c : path) {
        if (c == '\\')
            c = '/';
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        if (c == '/' && !out.empty() && out.back() == '/')
            continue;
        out.push_back(c);
    }
    return out;
}
inline uint64_t crc64(std::string_view s) {
    uint64_t c = 0xC96C5795D7870F42ull;
    for (unsigned char b : s)
        c = table[(c ^ b) & 0xff] ^ (c >> 8);
    return c;
}
// The game's asset id of a path (the toc's AssetIds hold these).
inline uint64_t assetId(std::string_view path) { return (crc64(normalize(path)) >> 2) | 0x8000000000000000ull; }

// A model asset path at text (max bytes readable): printable path characters, a '/' or '\', NUL-ended within
// max, ending ".model". Returns its length, or 0.
inline size_t modelPath(const char* text, size_t max) {
    size_t n = 0;
    bool slash = false;
    for (; n < max; ++n) {
        const char c = text[n];
        if (c == 0)
            break;
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                        c == '-' || c == '.' || c == '/' || c == '\\';
        if (!ok)
            return 0;
        slash = slash || c == '/' || c == '\\';
    }
    if (n == max || n < 8 || !slash)
        return 0;
    const std::string tail = normalize(std::string_view(text + n - 6, 6));
    return tail == ".model" ? n : 0;
}

constexpr uint64_t lowest = 0x10000, highest = 0x00007FFFFFFFFFFFull; // a user-mode address
inline bool plausible(uint64_t p) { return p >= lowest && p < highest; }

enum How : uint8_t { direct = 0, viaObject = 1 };
struct Hit {
    uint8_t root{};   // 0 the record (actor), 1 its instance
    uint8_t how{};    // direct: root+off1 is a char*; viaObject: root+off1 -> object, object+off2 is a char*
    uint16_t off1{}, off2{};
    uint64_t id{};
    std::string path; // normalized
};
struct Limits {
    uint32_t words = 0x80; // words of each root (0x400 bytes)
    uint32_t deep = 0x20;  // words of each object they point at (0x100 bytes)
    size_t text = 160;     // longest path read
};

// Reads a model path at p through peek(address, out, n) -> bool; a short second read near a page end.
template <class Peek> bool readPath(Peek& peek, uint64_t p, const Limits& lim, std::string& out) {
    if (!plausible(p))
        return false;
    char buf[256]{};
    const size_t want = std::min<size_t>(lim.text, sizeof buf - 1);
    size_t got = 0;
    if (peek(p, buf, want))
        got = want;
    else {
        const size_t toPage = 0x1000 - static_cast<size_t>(p & 0xfff);
        if (toPage < want && peek(p, buf, toPage))
            got = toPage;
    }
    if (!got)
        return false;
    const size_t n = modelPath(buf, got);
    if (!n)
        return false;
    out = normalize(std::string_view(buf, n));
    return true;
}

// Every model path one or two pointers from the roots (record, instance), each path once (its first place).
template <class Peek> std::vector<Hit> scan(const uint64_t roots[2], Peek peek, const Limits& lim = {}) {
    std::vector<Hit> hits;
    auto keep = [&](Hit h) {
        for (const auto& k : hits)
            if (k.path == h.path)
                return;
        h.id = assetId(h.path);
        hits.push_back(std::move(h));
    };
    for (uint8_t r = 0; r < 2; ++r) {
        if (!plausible(roots[r]) || (roots[r] & 7))
            continue;
        std::vector<uint64_t> words(lim.words);
        if (!peek(roots[r], words.data(), words.size() * 8))
            continue;
        for (uint32_t i = 0; i < lim.words; ++i) {
            const uint64_t p = words[i];
            if (!plausible(p))
                continue;
            Hit h{r, direct, static_cast<uint16_t>(i * 8), 0, 0, {}};
            if (readPath(peek, p, lim, h.path)) {
                keep(h);
                continue;
            }
            if (p & 7)
                continue;
            std::vector<uint64_t> inner(lim.deep);
            if (!peek(p, inner.data(), inner.size() * 8))
                continue;
            for (uint32_t j = 0; j < lim.deep; ++j) {
                Hit d{r, viaObject, static_cast<uint16_t>(i * 8), static_cast<uint16_t>(j * 8), 0, {}};
                if (readPath(peek, inner[j], lim, d.path))
                    keep(d);
            }
        }
    }
    return hits;
}

// The console lines for a new set of hits (empty when the set of paths did not change).
class Watch {
public:
    std::vector<std::string> update(bool on, uint64_t record, const std::vector<Hit>& hits) {
        std::vector<std::string> lines;
        if (!on) {
            if (was)
                lines.push_back("SUIT PROBE: off");
            was = false;
            last.clear();
            state = 0;
            return lines;
        }
        if (!was)
            lines.push_back("SUIT PROBE: on (read-only; a line each time the worn .model files change)");
        was = true;
        if (!record) {
            if (state != 1)
                lines.push_back("SUIT PROBE: no player yet");
            state = 1;
            last.clear();
            return lines;
        }
        std::vector<std::string> now;
        for (const auto& h : hits)
            now.push_back(h.path);
        std::sort(now.begin(), now.end());
        if (state == 2 && now == last)
            return lines;
        state = 2;
        last = now;
        ++changes;
        if (hits.empty()) {
            lines.push_back("SUIT PROBE: no .model path near the player (CANDIDATE layout missed; send this log)");
            return lines;
        }
        lines.push_back("SUIT PROBE: change " + std::to_string(changes) + ", " + std::to_string(hits.size()) +
                        " model path(s):");
        size_t shown = 0;
        for (const auto& h : hits) {
            if (++shown > 16) {
                lines.push_back("SUIT PROBE: ... " + std::to_string(hits.size() - 16) + " more");
                break;
            }
            lines.push_back("SUIT PROBE: " + h.path + " id " + hex(h.id) + " at " + where(h));
        }
        return lines;
    }
    static std::string hex(uint64_t v, int digits = 16) {
        static const char* d = "0123456789abcdef";
        std::string s = "0x";
        for (int i = digits - 1; i >= 0; --i)
            s.push_back(d[(v >> (i * 4)) & 15]);
        return s;
    }
    static std::string where(const Hit& h) {
        std::string s = h.root ? "instance+" : "record+";
        s += hex(h.off1, 3);
        if (h.how == viaObject)
            s += " -> +" + hex(h.off2, 3);
        return s;
    }
    uint32_t changes{};

private:
    bool was{};
    int state{}; // 1 no player, 2 a set logged
    std::vector<std::string> last;
};

// src/suit_probe.cpp (Windows): at most once a second while on, scans through an SEH peek and hands each new
// console line to log. Never throws.
void frame(uint64_t record, bool on, void (*log)(const char*)) noexcept;

} // namespace spidy::suit_probe
