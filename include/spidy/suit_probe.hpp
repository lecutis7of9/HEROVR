// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// 0.0.76 SUIT PROBE (live.txt suit_probe, export SpidySuitProbe 5553; the wrist HUD is full). READ-ONLY, the
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
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace spidy::suit_probe {

// The live.txt subtext (one sentence, its restart rule).
constexpr const char* help = "SUIT PROBE reads (never writes) the game's memory near your Spider-Man and logs the "
                             ".model files of the suit he wears, and with your sm2scan.json beside the launcher which "
                             "of your suits' ids it finds deeper, to the console when you change suit, for the "
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

// ---------------------------------------------------------------------------------------------------------
// 0.0.76 SUIT PROBE 2 (the same live.txt suit_probe; still READ-ONLY). Run 2 (pt1587u) showed the first walk
// only reaches the base body (hero_spiderman_base.model at record+0x030 -> +0x038, CONFIRMED there and
// CONFIRMED not to change with a suit). PROBE 2 takes the user's own sm2scan.json (every suit's loadout .config
// id, its .model ids and its mask's) and searches DEEPER from the player: breadth first from the record, its
// instance and the hero component, up to `depth` pointer steps (the child parts), and logs every word that IS
// one of those asset ids, and every .model/.config path whose id is one. Throttled: `perStep` objects a frame,
// a pass of at most `objects` objects, a pause between passes; a line only when the set of found ids changes.
// Every place it prints is CANDIDATE until a run shows it following the swaps.

// A tiny JSON reader (sm2scan.json only: objects, arrays, strings, numbers, true/false/null). Never throws out
// of parse(); a bad or too-deep file is an empty value.
struct Json {
    enum Type : uint8_t { isNull, isBool, isNumber, isString, isArray, isObject } type{isNull};
    std::string text; // a string's value (escapes kept simple: \uXXXX becomes '?')
    std::vector<Json> items;
    std::vector<std::pair<std::string, Json>> fields;
    const Json* get(std::string_view key) const {
        for (const auto& f : fields)
            if (f.first == key)
                return &f.second;
        return nullptr;
    }
    std::string str(std::string_view key) const {
        const Json* j = get(key);
        return j && j->type == isString ? j->text : std::string();
    }
    static Json parse(std::string_view s) {
        Json out;
        size_t i = 0;
        try {
            if (!value(s, i, out, 0))
                out = Json{};
        } catch (...) {
            out = Json{};
        }
        return out;
    }

private:
    static void space(std::string_view s, size_t& i) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t'))
            ++i;
    }
    static bool quoted(std::string_view s, size_t& i, std::string& out) {
        if (i >= s.size() || s[i] != '"')
            return false;
        ++i;
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c == '\\') {
                if (i >= s.size())
                    return false;
                const char e = s[i++];
                if (e == 'u') {
                    if (i + 4 > s.size())
                        return false;
                    i += 4;
                    c = '?';
                } else
                    c = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e == 'b' ? '\b' : e == 'f' ? '\f' : e;
            }
            out.push_back(c);
        }
        if (i >= s.size())
            return false;
        ++i;
        return true;
    }
    static bool value(std::string_view s, size_t& i, Json& out, int depth) {
        if (depth > 32)
            return false;
        space(s, i);
        if (i >= s.size())
            return false;
        const char c = s[i];
        if (c == '{') {
            out.type = isObject;
            ++i;
            space(s, i);
            if (i < s.size() && s[i] == '}')
                return ++i, true;
            for (;;) {
                space(s, i);
                std::string key;
                if (!quoted(s, i, key))
                    return false;
                space(s, i);
                if (i >= s.size() || s[i] != ':')
                    return false;
                ++i;
                Json v;
                if (!value(s, i, v, depth + 1))
                    return false;
                out.fields.emplace_back(std::move(key), std::move(v));
                space(s, i);
                if (i < s.size() && s[i] == ',') {
                    ++i;
                    continue;
                }
                if (i < s.size() && s[i] == '}')
                    return ++i, true;
                return false;
            }
        }
        if (c == '[') {
            out.type = isArray;
            ++i;
            space(s, i);
            if (i < s.size() && s[i] == ']')
                return ++i, true;
            for (;;) {
                Json v;
                if (!value(s, i, v, depth + 1))
                    return false;
                out.items.push_back(std::move(v));
                space(s, i);
                if (i < s.size() && s[i] == ',') {
                    ++i;
                    continue;
                }
                if (i < s.size() && s[i] == ']')
                    return ++i, true;
                return false;
            }
        }
        if (c == '"') {
            out.type = isString;
            return quoted(s, i, out.text);
        }
        for (const char* word : {"true", "false", "null"}) {
            const std::string_view w(word);
            if (s.substr(i, w.size()) == w) {
                out.type = w == "null" ? isNull : isBool;
                out.text = word;
                i += w.size();
                return true;
            }
        }
        const size_t from = i;
        while (i < s.size() && (s[i] == '-' || s[i] == '+' || s[i] == '.' || s[i] == 'e' || s[i] == 'E' ||
                                (s[i] >= '0' && s[i] <= '9')))
            ++i;
        if (i == from)
            return false;
        out.type = isNumber;
        out.text = std::string(s.substr(from, i - from));
        return true;
    }
};

// "BC46538AB3E876D2" -> its value; 0 for anything else.
inline uint64_t hexId(std::string_view s) {
    if (s.size() != 16)
        return 0;
    uint64_t v = 0;
    for (char c : s) {
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0)
            return 0;
        v = v << 4 | static_cast<uint64_t>(d);
    }
    return v;
}

// The ids PROBE 2 looks for, from the user's own sm2scan.json.
enum Kind : uint8_t { kindLoadout = 0, kindModel = 1, kindMask = 2 };
struct Target {
    Kind kind{};
    std::string path;            // normalized
    std::vector<uint16_t> suits; // indexes into Table::names (a model several suits share lists them all)
};
struct Table {
    std::vector<std::string> names; // each suit's name (its DisplayName key when it has no name)
    std::vector<std::pair<uint64_t, Target>> ids; // sorted by id
    const Target* find(uint64_t id) const {
        auto it = std::lower_bound(ids.begin(), ids.end(), id,
                                   [](const std::pair<uint64_t, Target>& e, uint64_t v) { return e.first < v; });
        return it != ids.end() && it->first == id ? &it->second : nullptr;
    }
    bool empty() const { return ids.empty(); }
    // sm2scan.json's text -> the table (empty for a file that is not one). Never throws.
    static Table fromScan(std::string_view text) {
        Table t;
        try {
            const Json root = Json::parse(text);
            const Json* suits = root.get("suits");
            if (!suits || suits->type != Json::isArray)
                return {};
            std::vector<std::pair<uint64_t, Target>> all;
            std::function<void(uint64_t, Kind, const std::string&, uint16_t)> add;
            add = [&](uint64_t id, Kind kind, const std::string& path, uint16_t suit) {
                if (!path.empty() && assetId(path) != id) { // the path's own id too (a file's id that differs)
                    const uint64_t own = assetId(path);
                    if (id)
                        add(own, kind, path, suit);
                    else
                        id = own;
                }
                if (!id)
                    return;
                for (auto& e : all)
                    if (e.first == id) {
                        if (std::find(e.second.suits.begin(), e.second.suits.end(), suit) == e.second.suits.end())
                            e.second.suits.push_back(suit);
                        return;
                    }
                all.push_back({id, Target{kind, normalize(path), {suit}}});
            };
            for (const auto& s : suits->items) {
                if (s.type != Json::isObject || t.names.size() >= 0xfff0)
                    continue;
                std::string name = s.str("name");
                if (name.empty())
                    name = s.str("display");
                if (name.empty())
                    name = "suit " + std::to_string(t.names.size() + 1);
                const auto index = static_cast<uint16_t>(t.names.size());
                t.names.push_back(name);
                const std::string item = s.str("item");
                add(hexId(s.str("id")), kindLoadout, item, index);
                if (const Json* models = s.get("models"); models && models->type == Json::isArray)
                    for (const auto& m : models->items)
                        if (m.type == Json::isObject)
                            add(hexId(m.str("id")), kindModel, m.str("path"), index);
                if (const Json* mask = s.get("mask"); mask && mask->type == Json::isObject)
                    add(hexId(mask->str("id")), kindMask, mask->str("path"), index);
            }
            std::sort(all.begin(), all.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            t.ids = std::move(all);
        } catch (...) {
            return {};
        }
        return t;
    }
};

// An asset path at text (max bytes readable): path characters (a space allowed, as in some mod suits), a '/'
// or '\', NUL-ended within max, ending ".model" or ".config". Returns its length, or 0.
inline size_t assetPath(const char* text, size_t max) {
    size_t n = 0;
    bool slash = false;
    for (; n < max; ++n) {
        const char c = text[n];
        if (c == 0)
            break;
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                        c == '-' || c == '.' || c == '/' || c == '\\' || c == ' ';
        if (!ok)
            return 0;
        slash = slash || c == '/' || c == '\\';
    }
    if (n == max || n < 10 || !slash)
        return 0;
    const std::string tail = normalize(std::string_view(text + n - 7, 7));
    return tail == ".config" || tail.substr(1) == ".model" ? n : 0;
}

struct DeepLimits {
    uint32_t words = 0x40;     // words read of each object (0x200 bytes)
    uint32_t depth = 5;        // pointer steps from a root
    uint32_t objects = 6000;   // objects a pass
    uint32_t perStep = 16;     // objects a frame (at most perStep * (words + 1) reads a frame)
    size_t text = 200;         // longest path read
    uint64_t skipLo = 0, skipHi = 0; // never walked into (the game's exe: vtables and code), still matched
    uint32_t faultsPerObject = 8;  // an object whose words mostly fail to read is not pointers: the rest skipped
    uint32_t faultsPerStep = 48;   // a frame stops early after this many failed reads
};

// One search pass, done a few objects at a time (step), breadth first.
class Deep {
public:
    struct Found {
        uint64_t id{};
        bool viaPath{}; // a path whose id it is, rather than the id itself
        std::string chain;
    };
    static constexpr const char* rootNames[3] = {"record", "instance", "hero"};
    void start(const uint64_t roots[3]) {
        nodes.clear();
        visited.clear();
        found.clear();
        next = 0;
        for (uint8_t r = 0; r < 3; ++r)
            if (plausible(roots[r]) && !(roots[r] & 7) && visited.insert(roots[r]).second)
                nodes.push_back(Node{roots[r], -1, 0, 0, r});
        running = !nodes.empty();
    }
    bool busy() const { return running; }
    size_t objects() const { return next; }
    const std::vector<Found>& results() const { return found; }
    // Reads up to lim.perStep objects; true when the pass is done (then results() is the whole pass).
    template <class Peek> bool step(Peek& peek, const Table& suits, const DeepLimits& lim) {
        if (!running)
            return false;
        std::vector<uint64_t> words(lim.words);
        uint32_t faults = 0;
        for (uint32_t k = 0; k < lim.perStep && next < nodes.size() && faults < lim.faultsPerStep; ++k) {
            const Node node = nodes[next];
            const int32_t me = static_cast<int32_t>(next++);
            uint32_t count = lim.words;
            if (!peek(node.addr, words.data(), count * 8)) {
                ++faults;
                const uint64_t toPage = 0x1000 - (node.addr & 0xfff);
                count = static_cast<uint32_t>(std::min<uint64_t>(toPage / 8, lim.words));
                if (!count || !peek(node.addr, words.data(), count * 8)) {
                    ++faults;
                    continue;
                }
            }
            uint32_t mine = 0; // this object's failed reads
            for (uint32_t i = 0; i < count && mine < lim.faultsPerObject; ++i) {
                const uint64_t w = words[i];
                if (!w)
                    continue;
                if (suits.find(w)) {
                    keep(w, false, chainOf(me) + " +" + hex3(i * 8));
                    continue;
                }
                if (!plausible(w))
                    continue;
                std::string path;
                const Text text = readAsset(peek, w, lim, path);
                if (text == unreadable) { // never walked into either
                    ++mine;
                    ++faults;
                    continue;
                }
                if (text == asset) {
                    const uint64_t id = assetId(path);
                    if (suits.find(id))
                        keep(id, true, chainOf(me) + " +" + hex3(i * 8) + " -> \"" + path + "\"");
                    continue;
                }
                if ((w & 7) || node.depth + 1u > lim.depth || nodes.size() >= lim.objects)
                    continue;
                if (lim.skipHi > lim.skipLo && w >= lim.skipLo && w < lim.skipHi)
                    continue;
                if (!visited.insert(w).second)
                    continue;
                nodes.push_back(Node{w, me, static_cast<uint16_t>(i * 8), static_cast<uint8_t>(node.depth + 1), node.root});
            }
        }
        if (next >= nodes.size())
            running = false;
        return !running;
    }

private:
    struct Node {
        uint64_t addr;
        int32_t parent;
        uint16_t off; // where in the parent the pointer to it was
        uint8_t depth, root;
    };
    static std::string hex3(uint32_t v) {
        static const char* d = "0123456789abcdef";
        std::string s = "0x";
        for (int i = 2; i >= 0; --i)
            s.push_back(d[(v >> (i * 4)) & 15]);
        return s;
    }
    std::string chainOf(int32_t n) const {
        std::vector<uint16_t> offs;
        int32_t at = n;
        while (at >= 0 && nodes[at].parent >= 0) {
            offs.push_back(nodes[at].off);
            at = nodes[at].parent;
        }
        std::string s = rootNames[at >= 0 ? nodes[at].root : 0];
        for (auto it = offs.rbegin(); it != offs.rend(); ++it)
            s += (it == offs.rbegin() ? "+" : " -> +") + hex3(*it);
        return s;
    }
    void keep(uint64_t id, bool viaPath, std::string chain) {
        for (const auto& f : found)
            if (f.id == id)
                return; // its nearest place only
        found.push_back(Found{id, viaPath, std::move(chain)});
    }
    enum Text { notText, asset, unreadable };
    template <class Peek> static Text readAsset(Peek& peek, uint64_t p, const DeepLimits& lim, std::string& out) {
        char head[8]{};
        if (!peek(p, head, 8)) // most pointers are not text: one short read decides
            return unreadable;
        for (char c : head) {
            const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                            c == '_' || c == '-' || c == '/' || c == '\\' || c == ' ' || c == '.';
            if (!ok)
                return notText;
        }
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
            return notText;
        const size_t n = assetPath(buf, got);
        if (!n)
            return notText;
        out = normalize(std::string_view(buf, n));
        return asset;
    }
    std::vector<Node> nodes;
    std::unordered_set<uint64_t> visited;
    std::vector<Found> found;
    size_t next{};
    bool running{};
};

// PROBE 2's console lines for a finished pass (empty when the set of found ids did not change).
class Watch2 {
public:
    std::vector<std::string> pass(const Table& suits, const std::vector<Deep::Found>& found, size_t objects) {
        std::vector<std::string> lines;
        std::vector<uint64_t> now;
        for (const auto& f : found)
            now.push_back(f.id);
        std::sort(now.begin(), now.end());
        ++passes;
        if (logged && now == last)
            return lines;
        logged = true;
        last = now;
        ++changes;
        const std::string head = "SUIT PROBE 2: change " + std::to_string(changes) + " (pass " +
                                 std::to_string(passes) + ", " + std::to_string(objects) + " objects): ";
        if (found.empty()) {
            lines.push_back(head + "none of the " + std::to_string(suits.names.size()) +
                            " suits' ids near the player (CANDIDATE layout missed; send this log)");
            return lines;
        }
        lines.push_back(head + std::to_string(found.size()) + " suit id(s), every place CANDIDATE:");
        // A suit named by an id only it has (a shared model, as a body several suits wear, names none alone).
        std::vector<uint16_t> alone;
        size_t shown = 0;
        for (const auto& f : found) {
            const Target* t = suits.find(f.id);
            if (!t)
                continue;
            if (t->suits.size() == 1 &&
                std::find(alone.begin(), alone.end(), t->suits[0]) == alone.end())
                alone.push_back(t->suits[0]);
            if (++shown > 12) {
                continue;
            }
            std::string who = t->suits.size() == 1 ? suits.names[t->suits[0]]
                                                   : "shared by " + std::to_string(t->suits.size()) + " suits";
            static const char* kinds[3] = {"loadout", "model", "mask"};
            lines.push_back("SUIT PROBE 2: " + who + " " + kinds[t->kind] + " " + t->path + " id " +
                            Watch::hex(f.id) + (f.viaPath ? " (its path)" : "") + " at " + f.chain);
        }
        if (shown > 12)
            lines.push_back("SUIT PROBE 2: ... " + std::to_string(shown - 12) + " more");
        if (alone.size() == 1)
            lines.push_back("SUIT PROBE 2: worn suit (CANDIDATE until it follows the swaps): " + suits.names[alone[0]]);
        else if (alone.size() > 1)
            lines.push_back("SUIT PROBE 2: " + std::to_string(alone.size()) + " suits named at once (not one worn suit)");
        return lines;
    }
    void reset() {
        logged = false;
        last.clear();
    }
    uint32_t changes{}, passes{};

private:
    bool logged{};
    std::vector<uint64_t> last;
};

// src/suit_probe.cpp (Windows): PROBE 1 at most once a second, PROBE 2 a few objects a frame (a pass, then a
// two-second pause), both through an SEH peek; sm2scan.json is read from the HERO VR folder or the Desktop.
// Each new console line goes to log. Never throws; off reads nothing.
void frame(uint64_t hero, uint64_t record, bool on, void (*log)(const char*)) noexcept;

} // namespace spidy::suit_probe
