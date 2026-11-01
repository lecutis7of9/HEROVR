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
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
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
// PROBE 3 watches these suits of run 4 (pt1611u's swaps, the names his sm2scan.json gives them) in every place.
constexpr const char* watchNames[3] = {"i30_Advanced_Suit", "SUIT_CLASSIC", "SUIT_NWH_HYBRID"}; // run 4's swaps
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
    std::vector<uint16_t> watchSuits; // the watchNames found in the scan
    std::vector<uint64_t> watchIds;   // their model and mask ids (each path's own id too), sorted
    bool watched(uint64_t id) const { return std::binary_search(watchIds.begin(), watchIds.end(), id); }
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
            for (const char* w : watchNames)
                for (size_t i = 0; i < t.names.size(); ++i)
                    if (t.names[i] == w) {
                        t.watchSuits.push_back(static_cast<uint16_t>(i));
                        break;
                    }
            for (const auto& e : t.ids)
                if (e.second.kind != kindLoadout)
                    for (uint16_t s : t.watchSuits)
                        if (std::find(e.second.suits.begin(), e.second.suits.end(), s) != e.second.suits.end()) {
                            t.watchIds.push_back(e.first);
                            break;
                        }
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

// 0.0.76 SUIT PROBE 3 (still the live.txt suit_probe; still READ-ONLY). Run 4 (pt1611u: Advanced Suit 2.0, then
// Classic, then Hybrid) found none of those three suits' ids: every pass hit its object cap, so each covered
// different memory and the found set flickered; most hits were loadout ids in one list (the unlocked suits);
// the 12-line cap hid the rest. PROBE 3: (1) a pass walks the same memory in a fixed order (roots, then word
// offsets) and reports only what it read completely (the depths the cap never cut); (2) a list holding
// listMin+ loadout ids is the unlocked suits: its ids are kept but hidden, and it is not walked into; (3) a
// change is reported only when two passes in a row agree, and then only what was added or removed; (4) an
// object holding a suit's model or mask id or a .model path (a loaded model) is read wider (modelWords) and
// walked modelExtra steps further; (5) the three suits of run 4 (watchNames, read from his sm2scan.json) get
// every place their model and mask ids are seen logged as WATCH lines. Every place is CANDIDATE.

struct DeepLimits {
    uint32_t words = 0x40;      // words read of each object (0x200 bytes)
    uint32_t modelWords = 0x80; // ... of a loaded-model object and its children (0x400 bytes)
    uint32_t depth = 6;         // pointer steps from a root
    uint32_t modelExtra = 2;    // steps below a loaded-model object even past depth
    uint32_t objects = 10000;   // objects a pass
    uint32_t perStep = 24;      // objects a frame (at most perStep * (modelWords * 2 + 2) reads a frame)
    uint32_t listMin = 3;       // an object holding this many loadout ids is a list (the unlocked suits)
    uint32_t placesPerId = 16;  // places kept of each watched id
    size_t text = 200;          // longest path read
    uint64_t skipLo = 0, skipHi = 0; // never walked into (the game's exe: vtables and code), still matched
    uint32_t faultsPerObject = 8;  // an object whose words mostly fail to read is not pointers: the rest skipped
    uint32_t faultsPerStep = 48;   // a frame stops early after this many failed reads
};

// One search pass, done a few objects at a time (step), breadth first in a fixed order.
class Deep {
public:
    struct Found {
        uint64_t id{};
        bool viaPath{}; // a path whose id it is, rather than the id itself
        bool inList{};  // seen only inside a list of loadout ids (the unlocked suits)
        std::string chain;
    };
    struct Place {
        uint64_t id{};
        std::string chain;
    };
    static constexpr uint8_t whole = 0xff;
    static constexpr const char* rootNames[3] = {"record", "instance", "hero"};
    void start(const uint64_t roots[3]) {
        nodes.clear();
        visited.clear();
        seen.clear();
        places.clear();
        outs.clear();
        outCount.clear();
        next = 0;
        cut = whole;
        for (uint8_t r = 0; r < 3; ++r)
            if (plausible(roots[r]) && !(roots[r] & 7) && visited.insert(roots[r]).second)
                nodes.push_back(Node{roots[r], -1, 0, 0, r, 0, false});
        running = !nodes.empty();
    }
    bool busy() const { return running; }
    size_t objects() const { return next; }
    // Objects shallower than this were all read (the cap cut deeper ones); whole: the cap was never reached.
    uint8_t complete() const { return cut; }
    // The ids found in the completely read depths, sorted by id (the same memory every pass).
    std::vector<Found> settled() const {
        std::vector<Found> out;
        for (const auto& s : seen) {
            const bool out1 = s.outDepth < cut, list1 = s.listDepth < cut;
            if (out1 || list1)
                out.push_back(Found{s.id, out1 ? s.outPath : s.listPath, !out1, out1 ? s.outChain : s.listChain});
        }
        std::sort(out.begin(), out.end(), [](const Found& a, const Found& b) { return a.id < b.id; });
        return out;
    }
    // Every place (up to placesPerId each) a watched id was seen in the completely read depths, in read order.
    std::vector<Place> watchedPlaces() const {
        std::vector<Place> out;
        for (const auto& p : places)
            if (p.depth < cut)
                out.push_back(Place{p.id, p.chain});
        return out;
    }
    // Every place (up to placesPerId each) a suit id was seen outside lists in the completely read depths, sorted
    // by id, then place (run 5: diffs by place, not by id - an id that moved or gained a place is told).
    std::vector<Found> settledPlaces() const {
        std::vector<Found> out;
        for (const auto& p : outs)
            if (p.depth < cut)
                out.push_back(Found{p.id, p.viaPath, false, p.chain});
        std::sort(out.begin(), out.end(),
                  [](const Found& a, const Found& b) { return a.id != b.id ? a.id < b.id : a.chain < b.chain; });
        return out;
    }
    // Reads up to lim.perStep objects; true when the pass is done (then settled() is the whole pass).
    template <class Peek> bool step(Peek& peek, const Table& suits, const DeepLimits& lim) {
        if (!running)
            return false;
        std::vector<uint64_t> words(std::max(lim.words, lim.modelWords));
        uint32_t faults = 0;
        for (uint32_t k = 0; k < lim.perStep && next < nodes.size() && faults < lim.faultsPerStep; ++k) {
            const Node node = nodes[next];
            const int32_t me = static_cast<int32_t>(next++);
            uint32_t count = read(peek, node.addr, words.data(), node.wide ? lim.modelWords : lim.words, faults);
            if (!count)
                continue;
            std::vector<Hit> hits;
            std::vector<std::pair<uint64_t, uint16_t>> kids; // pointers to walk, in word order
            bool model = false;
            uint32_t loadouts = 0, mine = 0;
            auto look = [&](uint32_t from, uint32_t to) {
                for (uint32_t i = from; i < to && mine < lim.faultsPerObject; ++i) {
                    const uint64_t w = words[i];
                    if (!w)
                        continue;
                    if (const Target* t = suits.find(w)) {
                        hits.push_back(Hit{w, false, static_cast<uint16_t>(i * 8), {}});
                        model = model || t->kind != kindLoadout;
                        loadouts += t->kind == kindLoadout ? 1u : 0u;
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
                        model = model || (path.size() > 6 && path.compare(path.size() - 6, 6, ".model") == 0);
                        const uint64_t id = assetId(path);
                        if (const Target* t = suits.find(id)) {
                            hits.push_back(Hit{id, true, static_cast<uint16_t>(i * 8), path});
                            loadouts += t->kind == kindLoadout ? 1u : 0u;
                        }
                        continue;
                    }
                    if (!(w & 7))
                        kids.emplace_back(w, static_cast<uint16_t>(i * 8));
                }
            };
            look(0, count);
            if (model && count < lim.modelWords) { // a loaded model: look further inside it
                const uint32_t more = read(peek, node.addr, words.data(), lim.modelWords, faults);
                if (more > count) {
                    look(count, more);
                    count = more;
                }
            }
            const bool list = loadouts >= lim.listMin;
            const std::string base = chainOf(me);
            for (auto& h : hits) {
                std::string chain = base + " +" + hex3(h.off) + (h.viaPath ? " -> \"" + h.path + "\"" : "");
                if (suits.watched(h.id))
                    place(h.id, node.depth, chain, lim);
                if (!list) // PROBE 3 (run 5): every place outside lists, so a new place of a known id is told
                    outPlace(h.id, h.viaPath, node.depth, chain, lim);
                keep(h.id, h.viaPath, list, node.depth, std::move(chain));
            }
            if (list) // the unlocked suits: its ids kept, never walked into
                continue;
            const uint32_t reach = std::max<uint32_t>(std::max<uint32_t>(lim.depth, node.reach),
                                                      model ? node.depth + lim.modelExtra : 0u);
            if (node.depth + 1u > reach)
                continue;
            for (const auto& [w, off] : kids) {
                if (lim.skipHi > lim.skipLo && w >= lim.skipLo && w < lim.skipHi)
                    continue;
                if (visited.count(w))
                    continue;
                if (nodes.size() >= lim.objects) { // the cap: what is this deep or deeper is not complete
                    cut = std::min<uint8_t>(cut, static_cast<uint8_t>(node.depth + 1));
                    break;
                }
                visited.insert(w);
                nodes.push_back(Node{w, me, off, static_cast<uint8_t>(node.depth + 1), node.root,
                                     static_cast<uint8_t>(std::min<uint32_t>(reach, 0xfe)), model || node.wide});
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
        uint8_t reach; // the deepest step its children may go (a loaded model's lets them past lim.depth)
        bool wide;     // under a loaded model: read modelWords
    };
    struct Hit {
        uint64_t id;
        bool viaPath;
        uint16_t off;
        std::string path;
    };
    struct Seen {
        uint64_t id{};
        uint8_t outDepth = whole, listDepth = whole; // its shallowest place outside a list, inside one
        bool outPath{}, listPath{};
        std::string outChain, listChain;
    };
    struct Placed {
        uint64_t id;
        uint8_t depth;
        std::string chain;
    };
    static std::string hex3(uint32_t v) {
        static const char* d = "0123456789abcdef";
        std::string s = "0x";
        for (int i = 2; i >= 0; --i)
            s.push_back(d[(v >> (i * 4)) & 15]);
        return s;
    }
    template <class Peek> static uint32_t read(Peek& peek, uint64_t addr, uint64_t* out, uint32_t want, uint32_t& faults) {
        if (peek(addr, out, want * 8))
            return want;
        ++faults;
        const uint64_t toPage = 0x1000 - (addr & 0xfff);
        const auto count = static_cast<uint32_t>(std::min<uint64_t>(toPage / 8, want));
        if (!count || !peek(addr, out, count * 8)) {
            ++faults;
            return 0;
        }
        return count;
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
    void keep(uint64_t id, bool viaPath, bool inList, uint8_t depth, std::string chain) {
        Seen* s = nullptr;
        for (auto& e : seen)
            if (e.id == id)
                s = &e;
        if (!s) {
            seen.emplace_back();
            s = &seen.back();
            s->id = id;
        }
        if (inList && depth < s->listDepth) // its shallowest place of each sort (the first read, breadth first)
            s->listDepth = depth, s->listPath = viaPath, s->listChain = std::move(chain);
        else if (!inList && depth < s->outDepth)
            s->outDepth = depth, s->outPath = viaPath, s->outChain = std::move(chain);
    }
    void place(uint64_t id, uint8_t depth, const std::string& chain, const DeepLimits& lim) {
        uint32_t n = 0;
        for (const auto& p : places)
            n += p.id == id ? 1u : 0u;
        if (n < lim.placesPerId)
            places.push_back(Placed{id, depth, chain});
    }
    void outPlace(uint64_t id, bool viaPath, uint8_t depth, const std::string& chain, const DeepLimits& lim) {
        uint32_t& n = outCount[id];
        if (n >= lim.placesPerId)
            return;
        ++n;
        outs.push_back(Out{id, depth, viaPath, chain});
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
    std::vector<Seen> seen;
    std::vector<Placed> places;
    struct Out {
        uint64_t id;
        uint8_t depth;
        bool viaPath;
        std::string chain;
    };
    std::vector<Out> outs;                          // every place outside lists (placesPerId each)
    std::unordered_map<uint64_t, uint32_t> outCount; // places kept of each id
    size_t next{};
    uint8_t cut{whole};
    bool running{};
};

// PROBE 3's console lines. A pass's set counts only when the pass before it found the same ids (two passes
// agree); then only what changed since the last set logged is printed (the first set in full, its list ids
// counted, not shown). WATCH lines: every place of the watched suits' model and mask ids, when they change.
class Watch2 {
public:
    static constexpr size_t maxLines = 40; // lines of one change at most (then "... N more")
    std::vector<std::string> pass(const Table& suits, const Deep& deep) {
        std::vector<std::string> lines;
        ++passes;
        const std::vector<Deep::Found> now = deep.settled();          // by id (lists, the worn suit)
        const std::vector<Deep::Found> nowAt = deep.settledPlaces(); // by place (run 5: what the lines diff)
        std::vector<std::string> sig;
        for (const auto& f : now)
            if (f.inList)
                sig.push_back("L " + Watch::hex(f.id));
        for (const auto& f : nowAt)
            sig.push_back(key(f));
        const bool agree = havePrev && sig == prev;
        prev = sig;
        havePrev = true;
        if (!agree || (logged && sig == lastSig))
            return lines;
        const bool first = !logged;
        const std::vector<Deep::Found> before = last, beforeAt = lastAt;
        logged = true;
        last = now;
        lastAt = nowAt;
        lastSig = sig;
        ++changes;
        const std::string head = std::string("SUIT PROBE 3: ") + (first ? "baseline" : "change " + std::to_string(changes - 1)) +
                                 " (passes " + std::to_string(passes - 1) + " and " + std::to_string(passes) +
                                 " agree, " + std::to_string(deep.objects()) + " objects, " + reach(deep) + "): ";
        if (now.empty() && first) {
            lines.push_back(head + "none of the " + std::to_string(suits.names.size()) +
                            " suits' ids near the player (CANDIDATE layout missed; send this log)");
            return lines;
        }
        std::vector<const Deep::Found*> added, removed;
        size_t listAdded = 0, listRemoved = 0, listNow = 0;
        for (const auto& f : now) {
            listNow += f.inList ? 1 : 0;
            if (f.inList && !has(before, f.id))
                ++listAdded;
        }
        for (const auto& f : before)
            if (f.inList && !has(now, f.id))
                ++listRemoved;
        for (const auto& f : nowAt)
            if (!hasPlace(beforeAt, f))
                added.push_back(&f);
        for (const auto& f : beforeAt)
            if (!hasPlace(nowAt, f))
                removed.push_back(&f);
        size_t outIds = 0;
        for (const auto& f : now)
            outIds += f.inList ? 0 : 1;
        if (first)
            lines.push_back(head + std::to_string(outIds) + " suit id(s) at " + std::to_string(nowAt.size()) +
                            " place(s); " + std::to_string(listNow) +
                            " loadout id(s) in lists (the unlocked suits) not shown; every place CANDIDATE:");
        else
            lines.push_back(head + "places +" + std::to_string(added.size()) + " -" + std::to_string(removed.size()) +
                            " (lists: +" + std::to_string(listAdded) + " -" + std::to_string(listRemoved) +
                            ", not shown); every place CANDIDATE:");
        size_t shown = 0, hidden = 0;
        auto say = [&](const char* sign, const Deep::Found& f, const char* at) {
            if (++shown > maxLines) {
                ++hidden;
                return;
            }
            lines.push_back(std::string("SUIT PROBE 3: ") + sign + " " + describe(suits, f.id, f.viaPath) + at + f.chain);
        };
        for (const auto* f : added)
            say(first ? "=" : "+", *f, " at ");
        for (const auto* f : removed)
            say("-", *f, " was at ");
        if (hidden)
            lines.push_back("SUIT PROBE 3: ... " + std::to_string(hidden) + " more");
        const std::vector<uint16_t> nowAlone = alone(suits, now), wasAlone = alone(suits, before);
        std::vector<uint16_t> gained, lost;
        for (uint16_t s : nowAlone)
            if (std::find(wasAlone.begin(), wasAlone.end(), s) == wasAlone.end())
                gained.push_back(s);
        for (uint16_t s : wasAlone)
            if (std::find(nowAlone.begin(), nowAlone.end(), s) == nowAlone.end())
                lost.push_back(s);
        if (first && nowAlone.size() == 1)
            lines.push_back("SUIT PROBE 3: worn suit (CANDIDATE until it follows the swaps): " + suits.names[nowAlone[0]]);
        else if (first && nowAlone.size() > 1)
            lines.push_back("SUIT PROBE 3: " + std::to_string(nowAlone.size()) +
                            " suits named outside lists at once (not one worn suit)");
        else if (!first && gained.size() == 1)
            lines.push_back("SUIT PROBE 3: swap (CANDIDATE until it follows the swaps): " +
                            (lost.size() == 1 ? suits.names[lost[0]] : std::string("?")) + " -> " +
                            suits.names[gained[0]]);
        return lines;
    }
    // The watched suits: every place, when the (agreed) set of places changes.
    std::vector<std::string> watch(const Table& suits, const Deep& deep) {
        std::vector<std::string> lines;
        if (suits.watchSuits.empty())
            return lines;
        const std::vector<Deep::Place> now = deep.watchedPlaces();
        std::vector<std::string> sig;
        for (const auto& p : now)
            sig.push_back(std::to_string(p.id) + " " + p.chain);
        std::sort(sig.begin(), sig.end());
        const bool agree = haveWatchPrev && sig == watchPrev;
        watchPrev = sig;
        haveWatchPrev = true;
        if (!agree || (watchLogged && sig == watchLast))
            return lines;
        watchLogged = true;
        watchLast = sig;
        ++watchChanges;
        lines.push_back("SUIT PROBE 3: WATCH " + std::to_string(watchChanges) + " (passes " + std::to_string(passes - 1) +
                        " and " + std::to_string(passes) + " agree, " + reach(deep) + "): " +
                        std::to_string(now.size()) + " place(s) of the watched suits' model and mask ids, every place CANDIDATE:");
        for (uint16_t s : suits.watchSuits) {
            size_t n = 0;
            for (const auto& p : now) {
                const Target* t = suits.find(p.id);
                if (!t || std::find(t->suits.begin(), t->suits.end(), s) == t->suits.end())
                    continue;
                ++n;
                lines.push_back("SUIT PROBE 3: WATCH " + suits.names[s] + " " + kinds[t->kind] + " " + t->path +
                                " id " + Watch::hex(p.id) + " at " + p.chain);
            }
            if (!n)
                lines.push_back("SUIT PROBE 3: WATCH " + suits.names[s] + ": none of its model or mask ids in the memory read");
        }
        return lines;
    }
    // A new player (a load): the next set needs two new passes, but changes are still against the last logged.
    void newPlayer() {
        havePrev = haveWatchPrev = false;
        prev.clear();
        watchPrev.clear();
    }
    void reset() { *this = Watch2{}; }
    uint32_t changes{}, passes{}, watchChanges{};

private:
    static constexpr const char* kinds[3] = {"loadout", "model", "mask"};
    static std::string reach(const Deep& deep) {
        return deep.complete() == Deep::whole ? std::string("every object in reach read")
                                              : "complete to depth " + std::to_string(deep.complete() - 1);
    }
    static bool has(const std::vector<Deep::Found>& v, uint64_t id) {
        const auto it = std::lower_bound(v.begin(), v.end(), id,
                                         [](const Deep::Found& f, uint64_t value) { return f.id < value; });
        return it != v.end() && it->id == id;
    }
    static std::string describe(const Table& suits, uint64_t id, bool viaPath) {
        const Target* t = suits.find(id);
        if (!t)
            return Watch::hex(id);
        const std::string who = t->suits.size() == 1 ? suits.names[t->suits[0]]
                                                     : "shared by " + std::to_string(t->suits.size()) + " suits";
        return who + " " + kinds[t->kind] + " " + t->path + " id " + Watch::hex(id) + (viaPath ? " (its path)" : "");
    }
    // Suits named outside lists by an id only they have.
    static std::vector<uint16_t> alone(const Table& suits, const std::vector<Deep::Found>& v) {
        std::vector<uint16_t> out;
        for (const auto& f : v) {
            const Target* t = f.inList ? nullptr : suits.find(f.id);
            if (t && t->suits.size() == 1 && std::find(out.begin(), out.end(), t->suits[0]) == out.end())
                out.push_back(t->suits[0]);
        }
        return out;
    }
    static std::string key(const Deep::Found& f) { return Watch::hex(f.id) + " " + f.chain; }
    static bool hasPlace(const std::vector<Deep::Found>& v, const Deep::Found& f) {
        for (const auto& e : v)
            if (e.id == f.id && e.chain == f.chain)
                return true;
        return false;
    }
    bool havePrev{}, logged{}, haveWatchPrev{}, watchLogged{};
    std::vector<std::string> prev, lastSig;
    std::vector<Deep::Found> last, lastAt;
    std::vector<std::string> watchPrev, watchLast;
};

// 0.0.76 SUIT PROBE 4 (still the live.txt suit_probe; still READ-ONLY). Run 5 (pt1611u: Advanced Suit 2.0,
// Classic, Hybrid) saw Classic and then Hybrid at record+0x080 -> +0x028 -> (an entry: +0x008 or +0x188) ->
// (+0x0e8, +0x0f0 or +0x128) +0x028, the entries moving around, but PROBE 3's deep pass took about 15 s and was
// cut at its object cap. PROBE 4 reads that container directly about once a second: the object B at
// record+0x080 -> +0x028 (words B words wide), every pointer in B (an entry E, entryWords wide), every suit id in E,
// and one step into every pointer in E (innerWords wide) for the ids there (+0x028 in run 5). Each id of the
// user's sm2scan.json found is a place "record+0x080 -> +0x028 -> +E +F +I"; the set of places is told when two
// passes agree, as what was added and removed (by place, as the entries move), then the suits it names. Every
// place is CANDIDATE until a run shows it following the three swaps in order.
struct ContainerLimits {
    uint32_t words = 0x80;       // words read of B (0x400 bytes: entries at +0x008 and +0x188 in run 5)
    uint32_t entryWords = 0x40;  // words read of each entry (0x200: +0x0e8, +0x0f0, +0x128 in run 5)
    uint32_t innerWords = 0x10;  // words read one step into an entry's pointer (0x80: +0x028 in run 5)
    uint32_t perStep = 8;        // entries read a frame (at most perStep * (entryWords + 2) reads a frame)
    uint32_t faultsPerStep = 64; // a frame stops early after this many failed reads
    uint32_t placesMost = 256;   // places kept a pass
    uint64_t skipLo = 0, skipHi = 0; // never walked into (the game's exe)
};

class Container {
public:
    static constexpr uint32_t headOff = 0x080, listOff = 0x028;
    enum State : uint8_t { idle, noHead, noList, reading, done };
    struct Place {
        uint64_t id{};
        std::string chain; // from B: "+0x188 -> +0x0e8 +0x028"
    };
    // A new pass over the container of this record (B read now, its entries in the steps that follow).
    template <class Peek> void start(Peek& peek, uint64_t record, const Table& suits, const ContainerLimits& lim) {
        places.clear();
        kids.clear();
        next = 0;
        entries = 0;
        b = 0;
        uint64_t head = 0;
        if (!plausible(record) || !peek(record + headOff, &head, 8) || !plausible(head) || (head & 7)) {
            state = noHead;
            return;
        }
        if (!peek(head + listOff, &b, 8) || !plausible(b) || (b & 7)) {
            b = 0;
            state = noList;
            return;
        }
        std::vector<uint64_t> w(lim.words);
        uint32_t faults = 0;
        const uint32_t n = readWords(peek, b, w.data(), lim.words, faults);
        if (!n) {
            state = noList;
            return;
        }
        for (uint32_t i = 0; i < n; ++i) {
            if (!w[i])
                continue;
            if (suits.find(w[i]))
                add(w[i], "+" + hex3(i * 8), lim);
            else if (plausible(w[i]) && !(w[i] & 7) && !(lim.skipHi > lim.skipLo && w[i] >= lim.skipLo && w[i] < lim.skipHi))
                kids.emplace_back(w[i], static_cast<uint16_t>(i * 8));
        }
        state = reading;
        if (kids.empty())
            state = done;
    }
    // Reads up to lim.perStep entries; true when the pass is done (then places() is the whole pass).
    template <class Peek> bool step(Peek& peek, const Table& suits, const ContainerLimits& lim) {
        if (state != reading)
            return state == done;
        std::vector<uint64_t> e(lim.entryWords), in(lim.innerWords);
        uint32_t faults = 0;
        for (uint32_t k = 0; k < lim.perStep && next < kids.size() && faults < lim.faultsPerStep; ++k) {
            const auto [addr, off] = kids[next++];
            const uint32_t n = readWords(peek, addr, e.data(), lim.entryWords, faults);
            if (!n)
                continue;
            ++entries;
            const std::string base = "+" + hex3(off) + " -> ";
            for (uint32_t j = 0; j < n && faults < lim.faultsPerStep; ++j) {
                const uint64_t w = e[j];
                if (!w)
                    continue;
                if (suits.find(w)) {
                    add(w, base + "+" + hex3(j * 8), lim);
                    continue;
                }
                if (!plausible(w) || (w & 7) || (lim.skipHi > lim.skipLo && w >= lim.skipLo && w < lim.skipHi))
                    continue;
                if (!peek(w, in.data(), lim.innerWords * 8)) { // one read: most words are not pointers to objects
                    ++faults;
                    continue;
                }
                for (uint32_t i = 0; i < lim.innerWords; ++i)
                    if (in[i] && suits.find(in[i]))
                        add(in[i], base + "+" + hex3(j * 8) + " +" + hex3(i * 8), lim);
            }
        }
        if (next >= kids.size())
            state = done;
        return state == done;
    }
    State status() const { return state; }
    uint64_t list() const { return b; }
    size_t entriesRead() const { return entries; }
    // The places of this pass, sorted by id, then place.
    std::vector<Place> found() const {
        std::vector<Place> out = places;
        std::sort(out.begin(), out.end(),
                  [](const Place& x, const Place& y) { return x.id != y.id ? x.id < y.id : x.chain < y.chain; });
        return out;
    }
    static std::string hex3(uint32_t v) {
        static const char* d = "0123456789abcdef";
        std::string s = "0x";
        for (int i = 2; i >= 0; --i)
            s.push_back(d[(v >> (i * 4)) & 15]);
        return s;
    }

private:
    template <class Peek> static uint32_t readWords(Peek& peek, uint64_t addr, uint64_t* out, uint32_t want, uint32_t& faults) {
        if (peek(addr, out, want * 8))
            return want;
        ++faults;
        const uint64_t toPage = 0x1000 - (addr & 0xfff);
        const auto count = static_cast<uint32_t>(std::min<uint64_t>(toPage / 8, want));
        if (!count || !peek(addr, out, count * 8)) {
            ++faults;
            return 0;
        }
        return count;
    }
    void add(uint64_t id, std::string chain, const ContainerLimits& lim) {
        if (places.size() < lim.placesMost)
            places.push_back(Place{id, std::move(chain)});
    }
    std::vector<Place> places;
    std::vector<std::pair<uint64_t, uint16_t>> kids;
    size_t next{}, entries{};
    uint64_t b{};
    State state{idle};
};

// PROBE 4's console lines: the container's state when it changes (not readable / readable again), and the set
// of places when two passes agree on a new one: what was added and removed, then the suits named.
class Watch4 {
public:
    static constexpr size_t maxLines = 40;
    static constexpr const char* where = "record+0x080 -> +0x028";
    std::vector<std::string> pass(const Table& suits, const Container& c) {
        std::vector<std::string> lines;
        ++passes;
        const Container::State st = c.status();
        if (st != lastState && (st == Container::noHead || st == Container::noList || lastState == Container::noHead ||
                                lastState == Container::noList || lastState == Container::idle)) {
            if (st == Container::noHead)
                lines.push_back(std::string("SUIT PROBE 4: record+0x080 is not a readable pointer (") + where +
                                " CANDIDATE; not read)");
            else if (st == Container::noList)
                lines.push_back(std::string("SUIT PROBE 4: ") + where + " is not readable (CANDIDATE; not read)");
            else
                lines.push_back(std::string("SUIT PROBE 4: reading ") + where + " = " + Watch::hex(c.list()) +
                                " about once a second (every place CANDIDATE)");
        }
        lastState = st;
        if (st != Container::done) {
            havePrev = false;
            return lines;
        }
        const std::vector<Container::Place> now = c.found();
        std::vector<std::string> sig;
        for (const auto& p : now)
            sig.push_back(Watch::hex(p.id) + " " + p.chain);
        const bool agree = havePrev && sig == prev;
        prev = sig;
        havePrev = true;
        if (!agree || (logged && sig == lastSig))
            return lines;
        const bool first = !logged;
        const std::vector<Container::Place> before = last;
        logged = true;
        last = now;
        lastSig = sig;
        ++changes;
        std::vector<const Container::Place*> added, removed;
        for (const auto& p : now)
            if (!has(before, p))
                added.push_back(&p);
        for (const auto& p : before)
            if (!has(now, p))
                removed.push_back(&p);
        lines.push_back(std::string("SUIT PROBE 4: ") + (first ? "baseline" : "change " + std::to_string(changes - 1)) +
                        " (passes " + std::to_string(passes - 1) + " and " + std::to_string(passes) + " agree, " +
                        where + " = " + Watch::hex(c.list()) + ", " + std::to_string(c.entriesRead()) + " entries read): " +
                        (first ? std::to_string(now.size()) + " place(s)"
                               : "+" + std::to_string(added.size()) + " -" + std::to_string(removed.size())) +
                        ", every place CANDIDATE:");
        size_t shown = 0, hidden = 0;
        auto say = [&](const char* sign, const Container::Place& p, const char* at) {
            if (++shown > maxLines) {
                ++hidden;
                return;
            }
            lines.push_back(std::string("SUIT PROBE 4: ") + sign + " " + describe(suits, p.id) + at + where + " -> " + p.chain);
        };
        for (const auto* p : added)
            say(first ? "=" : "+", *p, " at ");
        for (const auto* p : removed)
            say("-", *p, " was at ");
        if (hidden)
            lines.push_back("SUIT PROBE 4: ... " + std::to_string(hidden) + " more");
        lines.push_back("SUIT PROBE 4: suits now: " + named(suits, now));
        return lines;
    }
    // A new player (a load) or the probe turned off: start again.
    void reset() { *this = Watch4{}; }
    uint32_t passes{}, changes{};

private:
    static bool has(const std::vector<Container::Place>& v, const Container::Place& p) {
        for (const auto& e : v)
            if (e.id == p.id && e.chain == p.chain)
                return true;
        return false;
    }
    static std::string describe(const Table& suits, uint64_t id) {
        static constexpr const char* kinds[3] = {"loadout", "model", "mask"};
        const Target* t = suits.find(id);
        if (!t)
            return Watch::hex(id);
        const std::string who = t->suits.size() == 1 ? suits.names[t->suits[0]]
                                                     : "shared by " + std::to_string(t->suits.size()) + " suits";
        return who + " " + kinds[t->kind] + " " + t->path + " id " + Watch::hex(id);
    }
    // Each suit named by an id only it has, with the kinds seen ("SUIT_CLASSIC (model, mask)"), in order of first place.
    static std::string named(const Table& suits, const std::vector<Container::Place>& v) {
        std::vector<std::pair<uint16_t, uint8_t>> seen; // suit, kind bits
        for (const auto& p : v) {
            const Target* t = suits.find(p.id);
            if (!t || t->suits.size() != 1)
                continue;
            bool found = false;
            for (auto& s : seen)
                if (s.first == t->suits[0]) {
                    s.second = static_cast<uint8_t>(s.second | (1u << t->kind));
                    found = true;
                }
            if (!found)
                seen.emplace_back(t->suits[0], static_cast<uint8_t>(1u << t->kind));
        }
        if (seen.empty())
            return "none";
        std::string out;
        for (const auto& [s, bits] : seen) {
            out += (out.empty() ? "" : ", ") + suits.names[s] + " (";
            std::string k;
            if (bits & 1)
                k += "loadout";
            if (bits & 2)
                k += std::string(k.empty() ? "" : ", ") + "model";
            if (bits & 4)
                k += std::string(k.empty() ? "" : ", ") + "mask";
            out += k + ")";
        }
        return out;
    }
    Container::State lastState{Container::idle};
    bool havePrev{}, logged{};
    std::vector<std::string> prev, lastSig;
    std::vector<Container::Place> last;
};

// Every probe line gets a time (run 5 had none): seconds since the probe was turned on and the local clock,
// "SUIT PROBE 4 [t+12.3 s 16:57:16.123]: ..." (a line without "SUIT PROBE" in front: the stamp in front).
inline std::string stamped(const std::string& line, uint64_t msSinceOn, unsigned hour, unsigned minute, unsigned second,
                           unsigned milli) {
    char t[64];
    std::snprintf(t, sizeof t, "[t+%llu.%01llu s %02u:%02u:%02u.%03u]", static_cast<unsigned long long>(msSinceOn / 1000),
                  static_cast<unsigned long long>((msSinceOn % 1000) / 100), hour % 24, minute % 60, second % 60, milli % 1000);
    const std::string head = "SUIT PROBE";
    if (line.compare(0, head.size(), head) != 0)
        return std::string(t) + " " + line;
    const size_t colon = line.find(':');
    if (colon == std::string::npos)
        return line + " " + t;
    return line.substr(0, colon) + " " + t + line.substr(colon);
}

// What the probe costs the VR thread: each frame's time inside frame() (microseconds), told every `every` ms
// while it is on: the frames, the mean and the peak of that stretch, and the total since it was turned on.
struct Cost {
    uint64_t frames{}, totalUs{}, peakUs{};       // since on
    uint64_t winFrames{}, winUs{}, winPeak{};     // since the last line
    void add(uint64_t us) {
        ++frames, totalUs += us, peakUs = std::max(peakUs, us);
        ++winFrames, winUs += us, winPeak = std::max(winPeak, us);
    }
    // The line, and the stretch starts again.
    std::string line(uint64_t msSinceOn) {
        char t[256];
        const double sec = static_cast<double>(msSinceOn) / 1000.0;
        std::snprintf(t, sizeof t,
                      "SUIT PROBE cost: %llu frames, mean %llu us, peak %llu us this stretch; since on %llu frames, "
                      "mean %llu us, peak %llu us, total %llu ms (%.2f ms a second)",
                      static_cast<unsigned long long>(winFrames),
                      static_cast<unsigned long long>(winFrames ? winUs / winFrames : 0),
                      static_cast<unsigned long long>(winPeak), static_cast<unsigned long long>(frames),
                      static_cast<unsigned long long>(frames ? totalUs / frames : 0),
                      static_cast<unsigned long long>(peakUs), static_cast<unsigned long long>(totalUs / 1000),
                      sec > 0 ? static_cast<double>(totalUs) / 1000.0 / sec : 0.0);
        winFrames = winUs = winPeak = 0;
        return t;
    }
    void reset() { *this = Cost{}; }
};

// src/suit_probe.cpp (Windows): PROBE 1 at most once a second, PROBE 2 a few objects a frame (a pass, then a
// two-second pause), PROBE 4 about once a second (perStep entries a frame), all through an SEH peek, each line
// stamped, and the cost told every 30 s; sm2scan.json is read from the HERO VR folder or the Desktop.
// Each new console line goes to log. Never throws; off reads nothing.
void frame(uint64_t hero, uint64_t record, bool on, void (*log)(const char*)) noexcept;

} // namespace spidy::suit_probe
