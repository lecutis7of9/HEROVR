// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
#include "spidy/holo_roster.hpp"
#include "spidy/hero072.hpp"
#include "spidy/hero_controls.hpp"
#include "spidy/overlay_text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>

namespace spidy::holo_roster {
namespace {
std::mutex gLock;
Data gData{};
uint32_t gVersion{};

using roster_page::Status;
// The header: MARVEL, DC, (SOON with COMING SOON on), REP, COLOR, and CLOSE at the right.
// 0.0.74 test 10: COLOR joined them, so each tab is as wide as its word at the tabs' big text and the room
// left over is shared out (their words keep the same size as before).
constexpr float tabText = .0046f, tabGap = .003f, tabPad = .004f;
// The SOON screen: its title and MARVEL / DC in the row band, then five small tiles a row.
constexpr float soonSubU0 = -.006f, soonSubMid0 = .037f, soonSubMid1 = .040f;
constexpr float soonTop = .062f, soonTileH = .039f;
constexpr uint32_t soonCols = 5;
// A hero page: BACK, his name, PLAY AS; then CONTROLS, OUTFITS, STATS; then the tab's rows.
constexpr float rowY0 = .065f, rowY1 = .076f, backU1 = -.061f, playU0 = .024f;
constexpr float subY0 = .051f, subY1 = .062f;
constexpr float contentTop = .047f, contentBottom = -.064f;
constexpr float helpTop = -.086f, helpH = .0033f, helpPitch = .0052f;
// The grid.
constexpr float tileGap = .004f, tileTop = .075f, tileH = .056f;
// REP.
constexpr float meterY0 = .054f, meterY1 = .0605f, logTop = .033f, pageY0 = -.081f, pageY1 = -.071f;
constexpr float filterU0 = .028f;

// A row of buttons from `u0` to `u1`, each as wide as its word at `h` plus its share of the room left.
struct Word {
    int id;
    const char* word;
};
uint32_t row(const Word* w, uint32_t k, float u0, float u1, float v0, float v1, float h, Button* out) {
    float words = 0;
    for (uint32_t i = 0; i < k; ++i)
        words += textWidth(w[i].word, h) + tabPad;
    const float spare = std::max(0.f, u1 - u0 - tabGap * static_cast<float>(k - 1) - words) / static_cast<float>(k);
    float u = u0;
    for (uint32_t i = 0; i < k; ++i) {
        const float width = textWidth(w[i].word, h) + tabPad + spare;
        out[i] = Button{w[i].id, u, v0, i + 1 == k ? u1 : u + width, v1, true};
        u += width + tabGap;
    }
    return k;
}
uint32_t count(uint32_t group) { return group == 0 ? marvelCount : dcCount; }
uint32_t heroAt(uint32_t group, uint32_t k) { return group == 0 ? marvelHeroes[k] : dcHeroes[k]; }
void tileBox(uint32_t group, uint32_t k, float& u0, float& v0, float& u1, float& v1) {
    const uint32_t cols = group == 0 ? 4 : 3;
    const float w = (right - left - tileGap * static_cast<float>(cols - 1)) / static_cast<float>(cols);
    u0 = left + static_cast<float>(k % cols) * (w + tileGap);
    u1 = u0 + w;
    v1 = tileTop - static_cast<float>(k / cols) * (tileH + tileGap);
    v0 = v1 - tileH;
}
// The screen drawn: SOON with COMING SOON off is the grid.
Screen shownOf(const State& s, const Data& d) { return s.screen == Screen::soon && !d.comingSoon ? Screen::grid : s.screen; }
uint32_t soonIn(uint32_t group) {
    uint32_t n = 0;
    for (const SoonHero& x : soonHeroes)
        n += x.group == group;
    return n;
}
// The k-th tile of `group`'s page: its soonHeroes index.
uint32_t soonAt(uint32_t group, uint32_t k) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < soonCount; ++i)
        if (soonHeroes[i].group == group && n++ == k)
            return i;
    return 0;
}
void soonBox(uint32_t group, uint32_t k, float& u0, float& v0, float& u1, float& v1) {
    // Five a row on MARVEL; DC's six three a row, centred, the same size.
    const uint32_t cols = group == 0 ? soonCols : 3;
    const float w = (right - left - tileGap * static_cast<float>(soonCols - 1)) / static_cast<float>(soonCols);
    const float span = w * static_cast<float>(cols) + tileGap * static_cast<float>(cols - 1);
    const float start = (left + right) / 2 - span / 2;
    u0 = start + static_cast<float>(k % cols) * (w + tileGap);
    u1 = u0 + w;
    v1 = soonTop - static_cast<float>(k / cols) * (soonTileH + tileGap);
    v0 = v1 - soonTileH;
}
uint32_t outfitRows(const Data& d, uint32_t hero) {
    return d.suitsKnown ? std::min(roster_page::suitsMost, d.suits.count[std::min(hero, heroes - 1)]) : 0u;
}
float outfitPitch(uint32_t n) { return std::min(.0136f, (contentTop - contentBottom) / static_cast<float>(std::max(1u, n))); }
uint32_t logHero(const State& s) { return s.filter < heroes ? s.filter : heroes; }
uint32_t logCount(const State& s, const Data& d) { return d.log.countFor(logHero(s)); }
float logMost(const State& s, const Data& d) {
    return static_cast<float>(logCount(s, d) > logRows ? logCount(s, d) - logRows : 0u);
}
const char* nameOf(uint32_t h) { return hero_modes::heroNames[std::min(h, heroes - 1)]; }
std::string initials(uint32_t h) {
    const std::string n = nameOf(h);
    std::string out(1, n[0]);
    for (size_t i = 1; i < n.size(); ++i)
        if ((n[i - 1] == ' ' || n[i - 1] == '-') && n[i] != ' ')
            out += n[i];
    return out.size() > 1 ? out.substr(0, 2) : n.substr(0, 2);
}
std::string statusText(const roster_page::Row& r) {
    switch (r.status) {
    case Status::inUse: return "IN USE";
    case Status::unlocked: return "UNLOCKED";
    case Status::lockedFinal: return "LOCKED: MAX REP";
    case Status::lockedRep: return "LOCKED: REP " + std::to_string(r.rep);
    case Status::missing: return "MODEL MISSING";
    }
    return "";
}
Role statusRole(Status s) {
    switch (s) {
    case Status::inUse: return Role::good;
    case Status::unlocked: return Role::text;
    case Status::lockedFinal:
    case Status::lockedRep: return Role::warn;
    case Status::missing: return Role::bad;
    }
    return Role::text;
}

struct Out {
    std::vector<Piece>& p;
    void fill(float u0, float v0, float u1, float v1, Role r, float a = 1, Vec3 c = {}) {
        Piece x;
        x.kind = Piece::fill;
        x.u0 = u0, x.v0 = v0, x.u1 = u1, x.v1 = v1, x.role = r, x.alpha = a, x.color = c;
        p.push_back(x);
    }
    void outline(float u0, float v0, float u1, float v1, Role r, float a = 1) {
        Piece x;
        x.kind = Piece::outline;
        x.u0 = u0, x.v0 = v0, x.u1 = u1, x.v1 = v1, x.role = r, x.alpha = a;
        p.push_back(x);
    }
    void disc(float u, float v, float radius, Vec3 c, int portrait = -1) {
        Piece x;
        x.kind = Piece::disc;
        x.u0 = u - radius, x.v0 = v - radius, x.u1 = u + radius, x.v1 = v + radius, x.role = Role::custom, x.color = c;
        x.portrait = portrait;
        p.push_back(x);
    }
    // Left from u0 (or centred between u0 and u1), fitted to their room.
    void text(const std::string& t, float u0, float v, float u1, float h, Role r, bool centred = false, Vec3 c = {}) {
        if (t.empty())
            return;
        Piece x;
        x.kind = Piece::text;
        x.height = fittedHeight(t, h, std::max(.001f, u1 - u0));
        x.u0 = centred ? (u0 + u1) / 2 - textWidth(t, x.height) / 2 : u0;
        x.u1 = x.u0 + textWidth(t, x.height);
        x.v0 = v;
        x.v1 = v + x.height;
        x.words = t;
        x.role = r;
        x.color = c;
        x.centred = centred;
        p.push_back(x);
    }
    // Right-aligned to u1.
    void textRight(const std::string& t, float u0, float v, float u1, float h, Role r) {
        const float fh = fittedHeight(t, h, std::max(.001f, u1 - u0));
        text(t, u1 - textWidth(t, fh), v, u1, fh, r);
    }
    void button(const Button& b, const std::string& label, bool hover, bool lit, float h = .0042f) {
        fill(b.u0, b.v0, b.u1, b.v1, lit ? Role::accent : Role::backing, lit ? .55f : .55f);
        outline(b.u0, b.v0, b.u1, b.v1, hover && b.enabled ? Role::line : lit ? Role::frame : Role::accent,
                b.enabled ? 1.f : .45f);
        text(label, b.u0 + .002f, (b.v0 + b.v1) / 2 - h / 2, b.u1 - .002f, h,
             !b.enabled ? Role::help : hover ? Role::line : Role::text, true);
    }
};
// `t` broken at spaces into lines no wider than `w` at `h` (a longer word is split).
std::vector<std::string> wrap(const std::string& t, float h, float w, uint32_t most) {
    std::vector<std::string> lines;
    std::string line;
    size_t i = 0;
    while (i < t.size() && lines.size() < most) {
        size_t j = t.find(' ', i);
        if (j == std::string::npos)
            j = t.size();
        const std::string word = t.substr(i, j - i);
        const std::string tryLine = line.empty() ? word : line + " " + word;
        if (textWidth(tryLine, h) <= w || line.empty()) {
            line = tryLine;
        } else {
            lines.push_back(line);
            line = word;
        }
        i = j + 1;
    }
    if (!line.empty() && lines.size() < most)
        lines.push_back(line);
    return lines;
}
} // namespace

uint32_t groupOf(uint32_t hero) {
    for (uint32_t h : dcHeroes)
        if (h == hero)
            return 1;
    return 0;
}
void publish(const Data& d) {
    std::lock_guard<std::mutex> g(gLock);
    gData = d;
    ++gVersion;
}
uint32_t latest(Data& out) {
    std::lock_guard<std::mutex> g(gLock);
    out = gData;
    return gVersion;
}
uint32_t version() {
    std::lock_guard<std::mutex> g(gLock);
    return gVersion;
}

uint32_t buttons(const State& s, const Data& d, Button (&out)[buttonsMost]) {
    uint32_t n = 0;
    if (s.screen == Screen::closed)
        return 0;
    const Screen shown = shownOf(s, d);
    {
        Word w[6];
        uint32_t k = 0;
        w[k++] = {tabMarvel, "MARVEL"};
        w[k++] = {tabDc, "DC"};
        if (d.comingSoon)
            w[k++] = {tabSoon, "SOON"};
        w[k++] = {tabRep, "REP"};
        w[k++] = {tabColor, "COLOR"};
        w[k++] = {closeButton, "CLOSE"};
        n += row(w, k, left, right, tabY0, tabY1, tabText, out + n);
    }
    if (shown == Screen::color)
        return n; // the COLOR page under the header is wrist_hud's
    if (shown == Screen::soon) {
        out[n++] = Button{soonMarvel, soonSubU0, rowY0, soonSubMid0, rowY1, true};
        out[n++] = Button{soonDc, soonSubMid1, rowY0, right, rowY1, true};
        const uint32_t g = std::min(s.soonGroup, 1u);
        for (uint32_t k = 0; k < soonIn(g) && n < buttonsMost; ++k) {
            Button b;
            b.id = soonTile + static_cast<int>(soonAt(g, k));
            soonBox(g, k, b.u0, b.v0, b.u1, b.v1);
            b.enabled = false; // not playable yet: pointed at, it says so; pressed, nothing
            out[n++] = b;
        }
        return n;
    }
    if (shown == Screen::grid) {
        const uint32_t g = std::min(s.group, 1u);
        for (uint32_t k = 0; k < count(g); ++k) {
            Button b;
            b.id = tile + static_cast<int>(heroAt(g, k));
            tileBox(g, k, b.u0, b.v0, b.u1, b.v1);
            out[n++] = b;
        }
        return n;
    }
    if (shown == Screen::hero) {
        const uint32_t h = std::min(s.hero, heroes - 1);
        out[n++] = Button{backButton, left, rowY0, backU1, rowY1, true};
        out[n++] = Button{playButton, playU0, rowY0, right, rowY1, d.playing != h};
        // 0.0.73 test 4: the sub-tabs, CONTROLS, OUTFITS, STATS and SETTINGS; 0.0.74 test 10: and MUSIC (the
        // hologram's one music player under them), each as wide as its word.
        const Word w[] = {{subControls, "CONTROLS"}, {subOutfits, "OUTFITS"}, {subStats, "STATS"}, {subSettings, "SETTINGS"},
                          {subMusic, "MUSIC"}};
        n += row(w, 5, left, right, subY0, subY1, tabText, out + n);
        if (s.tab == Tab::settings) {
            const uint32_t pages = std::max(1u, s.settingsPages);
            out[n++] = Button{settingsPrev, left, pageY0, -.030f, pageY1, s.settingsPage > 0};
            out[n++] = Button{settingsNext, .030f, pageY0, right, pageY1, s.settingsPage + 1 < pages};
        } else if (s.tab == Tab::outfits) {
            const uint32_t rows = outfitRows(d, h);
            const float pitch = outfitPitch(rows);
            for (uint32_t i = 0; i < rows && n < buttonsMost; ++i) {
                const Status st = d.suits.rows[h][i].status;
                const float v1 = contentTop - static_cast<float>(i) * pitch;
                out[n++] = Button{outfit + static_cast<int>(i), left, v1 - pitch + .0012f, right, v1,
                                  st == Status::unlocked || st == Status::inUse};
            }
        } else if (s.tab == Tab::stats) {
            out[n++] = Button{seeLog, left, pageY0, right, pageY1, true};
        }
        return n;
    }
    // REP
    out[n++] = Button{logFilter, filterU0, rowY0, right, rowY1, true};
    const bool more = logCount(s, d) > logRows;
    out[n++] = Button{logNewer, left, pageY0, -.044f, pageY1, more && s.logFirst > .5f};
    out[n++] = Button{logOlder, -.041f, pageY0, -.001f, pageY1, more && s.logFirst < logMost(s, d) - .5f};
    return n;
}
int buttonAt(const State& s, const Data& d, float u, float v) {
    Button bs[buttonsMost];
    const uint32_t n = buttons(s, d, bs);
    for (uint32_t i = 0; i < n; ++i)
        if (u >= bs[i].u0 && u <= bs[i].u1 && v >= bs[i].v0 && v <= bs[i].v1)
            return bs[i].id;
    return none;
}

std::string helpFor(const State& s, const Data& d, int id) {
    const uint32_t h = std::min(s.hero, heroes - 1);
    const std::string who = nameOf(h);
    switch (id) {
    case tabMarvel: return "Shows the MARVEL heroes, a tile each. No restart needed.";
    case tabDc: return "Shows the DC heroes, a tile each. No restart needed.";
    case tabRep: return "Shows your rep meter and the log of what changed it. No restart needed.";
    case closeButton: return "Closes the heroes and goes back to the hologram's page. No restart needed.";
    case backButton: return "Back to the hero tiles. No restart needed.";
    case playButton:
        return d.playing == h ? "You are playing " + who + " now." : "Switches you to " + who + " right now. No restart needed.";
    case subControls: return "How to play " + who + ": each move and its button or gesture.";
    case subOutfits: return "Pick " + who + "'s outfit; locked ones open with his rep. No restart needed.";
    case subStats: return "What the mod has counted for " + who + ": his rep, level and fights.";
    case subSettings: return who + "'s own settings, then INVINCIBLE and FLYING for every hero.";
    case settingsPrev: return "The previous page of settings. No restart needed.";
    case settingsNext: return "The next page of settings. No restart needed.";
    case seeLog: return "Opens the REP log with only " + who + "'s entries. No restart needed.";
    case logFilter: return "Shows every hero's entries, or only one hero's. No restart needed.";
    case logNewer: return "Scrolls to newer entries (or push the stick up). No restart needed.";
    case logOlder: return "Scrolls to older entries (or pull the stick down). No restart needed.";
    case tabSoon: return "Shows the heroes coming in 0.0.74; they are not playable yet.";
    case tabColor: return "Shows the COLOR settings: the hologram's look, the filters and their strength. No restart needed.";
    case subMusic: return "Your music player: play, pause, skip and pick songs from Documents\\HERO VR\\Music.";
    case soonMarvel: return "Shows the MARVEL heroes coming in 0.0.74. No restart needed.";
    case soonDc: return "Shows the DC heroes and suits coming in 0.0.74. No restart needed.";
    default: break;
    }
    if (id >= soonTile && id < soonTile + static_cast<int>(soonCount))
        return std::string(soonHeroes[id - soonTile].name) + ": coming in 0.0.74, not playable yet.";
    if (id >= tile && id < tile + static_cast<int>(heroes))
        return std::string("Opens ") + nameOf(static_cast<uint32_t>(id - tile)) + "'s page: controls, outfits, stats, settings, music.";
    if (id >= outfit && d.suitsKnown && static_cast<uint32_t>(id - outfit) < outfitRows(d, h)) {
        const uint32_t i = static_cast<uint32_t>(id - outfit);
        const roster_page::Row& r = d.suits.rows[h][i];
        if (i == 0)
            return "Keeps the suit the game or your own suit mod gives him. No restart needed.";
        switch (r.status) {
        case Status::missing: return "This outfit's model is not downloaded yet, so it cannot be worn.";
        case Status::lockedFinal: return "His FINAL outfit: it unlocks at max rep (or TEST UNLOCKS).";
        case Status::lockedRep: return "Unlocks at " + std::to_string(r.rep) + " rep as " + who + ".";
        case Status::inUse: return "This is his outfit now, kept with your progress.";
        case Status::unlocked: return "Makes this his outfit, kept with your progress. No restart needed.";
        }
    }
    if (shownOf(s, d) == Screen::soon)
        return "Coming in 0.0.74: not playable yet.";
    if (shownOf(s, d) == Screen::grid)
        return "Point at a hero and pull the trigger to open his page.";
    if (s.screen == Screen::rep)
        return "Push the stick to scroll the log. Kept across sessions.";
    return "Point and pull the trigger to pick. CLOSE goes back to the hologram.";
}

void pieces(const State& s, const Data& d, std::vector<Piece>& p) {
    if (s.screen == Screen::closed)
        return;
    Out o{p};
    Button bs[buttonsMost];
    const uint32_t n = buttons(s, d, bs);
    const auto byId = [&](int id) -> const Button* {
        for (uint32_t i = 0; i < n; ++i)
            if (bs[i].id == id)
                return &bs[i];
        return nullptr;
    };
    const auto tabBtn = [&](int id, const char* label, bool lit) {
        if (const Button* b = byId(id))
            o.button(*b, label, s.hover == id, lit, .0046f);
    };
    const Screen shown = shownOf(s, d);
    const bool heroes2 = shown == Screen::grid || shown == Screen::hero;
    tabBtn(tabMarvel, "MARVEL", heroes2 && (shown == Screen::grid ? s.group == 0 : groupOf(s.hero) == 0));
    tabBtn(tabDc, "DC", heroes2 && (shown == Screen::grid ? s.group == 1 : groupOf(s.hero) == 1));
    tabBtn(tabSoon, "SOON", shown == Screen::soon);
    tabBtn(tabRep, "REP", shown == Screen::rep);
    tabBtn(tabColor, "COLOR", shown == Screen::color);
    tabBtn(closeButton, "CLOSE", false);
    const bool hostsPage = hosted(s) != Hosted::none; // wrist_hud draws that page's rows and its help
    const uint32_t playing = std::min(d.playing, heroes - 1);
    if (shown == Screen::color) {
        // The COLOR page's rows are wrist_hud's, under the header.
    } else if (shown == Screen::soon) {
        // 0.0.73 COMING SOON: the title, MARVEL / DC, and a small tile a hero: his picture (the Roster
        // model's soon_* picture) or, without it, a plain head-and-shoulders, and his name. Dim: not playable.
        const uint32_t g = std::min(s.soonGroup, 1u);
        o.text("COMING SOON", left, rowY0 + .0028f, soonSubU0 - .003f, .0056f, Role::text);
        tabBtn(soonMarvel, "MARVEL", g == 0);
        tabBtn(soonDc, "DC", g == 1);
        float lowest = soonTop;
        for (uint32_t k = 0; k < soonIn(g); ++k) {
            const uint32_t i = soonAt(g, k);
            const Button* b = byId(soonTile + static_cast<int>(i));
            if (!b)
                continue;
            lowest = std::min(lowest, b->v0);
            const bool hov = s.hover == b->id;
            const int pic = soonPicture(i);
            o.fill(b->u0, b->v0, b->u1, b->v1, Role::backing, .6f);
            o.outline(b->u0, b->v0, b->u1, b->v1, hov ? Role::line : Role::accent, hov ? 1.f : .6f);
            const float cu = (b->u0 + b->u1) / 2, cv = b->v1 - .0145f, r = .0115f;
            o.disc(cu, cv, r, Vec3{.10f, .13f, .19f}, pic);
            // No picture: a silhouette in its place (drawn only then).
            o.disc(cu, cv + r * .28f, r * .36f, Vec3{.42f, .47f, .56f}, pic);
            o.p.back().fallback = true;
            o.fill(cu - r * .62f, cv - r * .78f, cu + r * .62f, cv - r * .16f, Role::custom, 1.f, Vec3{.42f, .47f, .56f});
            o.p.back().portrait = pic, o.p.back().fallback = true;
            const float nameH = .0030f, roomW = b->u1 - b->u0 - .003f;
            const auto lines = wrap(soonHeroes[i].name, nameH, roomW, 2);
            const float first = lines.size() > 1 ? b->v1 - .0310f : b->v1 - .0335f;
            for (size_t l = 0; l < lines.size(); ++l)
                o.text(lines[l], b->u0 + .0015f, first - static_cast<float>(l) * .0042f, b->u1 - .0015f, nameH,
                       hov ? Role::line : Role::text, true);
        }
        if (g == 1) {
            const auto lines = wrap(soonSuits, .0032f, right - left, 2);
            for (size_t l = 0; l < lines.size(); ++l)
                o.text(lines[l], left, lowest - .012f - static_cast<float>(l) * .0058f, right, .0032f, Role::label);
        }
        o.text("NOT PLAYABLE YET - COMING IN 0.0.74", left, -.075f, right, .0032f, Role::value, true);
    } else if (shown == Screen::grid) {
        const uint32_t g = std::min(s.group, 1u);
        for (uint32_t k = 0; k < count(g); ++k) {
            const uint32_t h = heroAt(g, k);
            const Button* b = byId(tile + static_cast<int>(h));
            if (!b)
                continue;
            const bool hov = s.hover == b->id, now = h == playing;
            o.fill(b->u0, b->v0, b->u1, b->v1, now ? Role::accent : Role::backing, now ? .35f : .6f);
            o.outline(b->u0, b->v0, b->u1, b->v1, hov ? Role::line : now ? Role::good : Role::accent);
            const hero072::Bio& bio = hero072::bio(h);
            const Vec3 c{bio.color[0], bio.color[1], bio.color[2]};
            const float cu = (b->u0 + b->u1) / 2, cv = b->v1 - .0175f;
            // 0.0.73 test 3 PORTRAITS: the hero's portrait (the old popup's, the Roster model's picture h)
            // where the disc is; the disc and its initials when there is none.
            o.disc(cu, cv, .0115f, c, static_cast<int>(h));
            o.text(initials(h), cu - .011f, cv - .0026f, cu + .011f, .0052f, Role::custom, true, Vec3{1, 1, 1});
            o.p.back().portrait = static_cast<int>(h), o.p.back().fallback = true;
            o.text(bio.name, b->u0 + .0015f, b->v1 - .039f, b->u1 - .0015f, .0042f, hov ? Role::line : Role::text, true);
            const std::string sub = now ? std::string("PLAYING") : rep_log::rankText(d.heroPoints[h]);
            o.text(sub, b->u0 + .002f, b->v1 - .049f, b->u1 - .002f, .0031f, now ? Role::good : Role::value, true);
        }
        o.text("PLAYING: " + std::string(nameOf(playing)), left, -.054f, right, .0042f, Role::label);
        if (d.reputation)
            o.text(rep_log::rankText(d.heroPoints[playing]) + "   " + rep_log::nextText(d.heroPoints[playing]), left, -.064f,
                   right, .0036f, Role::value);
    } else if (shown == Screen::hero) {
        const uint32_t h = std::min(s.hero, heroes - 1);
        if (const Button* b = byId(backButton))
            o.button(*b, "BACK", s.hover == backButton, false);
        o.text(nameOf(h), backU1 + .003f, rowY0 + .0028f, playU0 - .003f, .0058f, Role::text, true);
        if (const Button* b = byId(playButton))
            o.button(*b, b->enabled ? "PLAY AS" : "PLAYING", s.hover == playButton, !b->enabled);
        tabBtn(subControls, "CONTROLS", s.tab == Tab::controls);
        tabBtn(subOutfits, "OUTFITS", s.tab == Tab::outfits);
        tabBtn(subStats, "STATS", s.tab == Tab::stats);
        tabBtn(subSettings, "SETTINGS", s.tab == Tab::settings);
        tabBtn(subMusic, "MUSIC", s.tab == Tab::music);
        if (s.tab == Tab::music) {
            // The player is wrist_hud's MUSIC page, under the sub-tabs.
        } else if (s.tab == Tab::settings) {
            // The rows are wrist_hud's (drawn over this); here only PREV, the page and NEXT.
            const uint32_t pages = std::max(1u, s.settingsPages);
            if (const Button* b = byId(settingsPrev))
                o.button(*b, "PREV", s.hover == settingsPrev, false, .0036f);
            if (const Button* b = byId(settingsNext))
                o.button(*b, "NEXT", s.hover == settingsNext, false, .0036f);
            o.text("PAGE " + std::to_string(std::min(s.settingsPage, pages - 1) + 1) + " OF " + std::to_string(pages), -.028f,
                   (pageY0 + pageY1) / 2 - .0018f, .028f, .0036f, Role::value, true);
        } else if (s.tab == Tab::controls) {
            const hero_controls::Guide& g = hero_controls::guideOf(h);
            const uint32_t lines = std::min(g.count, hero_controls::linesMost);
            const float pitch = std::min(.0124f, (contentTop - contentBottom - .004f) / static_cast<float>(std::max(1u, lines)));
            for (uint32_t i = 0; i < lines; ++i) {
                const float v1 = contentTop - static_cast<float>(i) * pitch, v0 = v1 - pitch + .001f;
                if (i % 2 == 0)
                    o.fill(left, v0, right, v1, Role::backing, .45f);
                const float base = (v0 + v1) / 2 - .0017f;
                o.text(g.lines[i].name, left + .002f, base, left + .046f, .0034f, Role::label);
                o.text(g.lines[i].how, left + .048f, base, right - .002f, .0034f, Role::text);
            }
        } else if (s.tab == Tab::outfits) {
            if (!d.suitsKnown) {
                o.text("READING THE OUTFITS...", left, contentTop - .01f, right, .0042f, Role::help);
            } else {
                const uint32_t rows = outfitRows(d, h);
                for (uint32_t i = 0; i < rows; ++i) {
                    const Button* b = byId(outfit + static_cast<int>(i));
                    if (!b)
                        continue;
                    const roster_page::Row& r = d.suits.rows[h][i];
                    const bool hov = s.hover == b->id;
                    o.fill(b->u0, b->v0, b->u1, b->v1, Role::backing, hov ? .8f : .5f);
                    o.outline(b->u0, b->v0, b->u1, b->v1, hov && b->enabled ? Role::line : Role::accent, b->enabled ? .9f : .4f);
                    if (r.status == Status::inUse)
                        o.fill(b->u0, b->v0, b->u0 + .0015f, b->v1, Role::good);
                    const std::string state = i == 0 && d.suits.ownSuit ? "IN USE (OWN SUIT)" : statusText(r);
                    const float stateH = .0030f, stateW = textWidth(state, stateH);
                    const float mid = (b->v0 + b->v1) / 2;
                    const std::string name = i == 0 ? std::string("OWN MOD OR GAME SUIT") : std::string(r.name) + (r.final ? " - FINAL" : "");
                    const std::string by = i == 0 ? std::string("THE GAME'S OWN, OR YOUR SUIT MOD'S")
                                                  : std::string("BY ") + r.artist + (r.licence[0] ? std::string(" (") + r.licence + ")" : "");
                    o.text(name, b->u0 + .003f, mid + .0004f, b->u1 - stateW - .006f, .0034f, hov ? Role::line : Role::text);
                    o.text(by, b->u0 + .003f, mid - .0044f, b->u1 - stateW - .006f, .0024f, Role::help);
                    o.textRight(state, b->u1 - stateW - .004f, mid - .0015f, b->u1 - .002f, stateH, statusRole(r.status));
                }
                if (d.suits.ownSuit)
                    o.text("OWN SUIT IS ON: EVERY HERO KEEPS HIS OWN MOD OR GAME SUIT", left, contentBottom - .012f, right, .003f,
                           Role::warn);
            }
        } else if (s.tab == Tab::stats) {
            const int32_t pts = d.heroPoints[h];
            const rep_log::Meter m = rep_log::meterOf(pts);
            const rep_log::Counts& c = d.log.counts(h);
            float v = contentTop - .0065f;
            const auto row = [&](const std::string& label, const std::string& value, Role r = Role::value) {
                o.text(label, left + .002f, v, -.008f, .0036f, Role::label);
                o.text(value, -.004f, v, right, .0036f, r);
                v -= .0102f;
            };
            if (!d.reputation) {
                row("REP", "REPUTATION IS OFF", Role::help);
            } else {
                row("REP", std::to_string(pts) + " OF " + std::to_string(hero072::repMost));
                row("RANK", rep_log::rankText(pts) + (m.max ? "  (MAX)" : ""));
                row("LEVEL", rep_log::nextText(pts));
                o.fill(left + .002f, v + .001f, right, v + .005f, Role::backing, .7f);
                o.fill(left + .002f, v + .001f, left + .002f + (right - left - .002f) * m.progress, v + .005f, Role::accent);
                v -= .0075f;
            }
            if (!d.repLog) {
                row("FIGHTS", "REP LOG IS OFF", Role::help);
            } else {
                row("BAD GUYS DOWN", std::to_string(c.downed));
                row("CRIMES STOPPED", std::to_string(c.stopped));
                row("CRIMES FAILED", std::to_string(c.failed));
                row("REP GAINED / LOST", "+" + std::to_string(c.gained) + " / -" + std::to_string(c.lost));
            }
            uint32_t open = 0, all = outfitRows(d, h);
            for (uint32_t i = 0; i < all; ++i)
                open += d.suits.rows[h][i].status == Status::inUse || d.suits.rows[h][i].status == Status::unlocked;
            if (d.suitsKnown)
                row("OUTFITS OPEN", std::to_string(open) + " OF " + std::to_string(all));
            o.text("COUNTED BY THE MOD SINCE 0.0.73, KEPT ACROSS SESSIONS", left, contentBottom - .006f, right, .0027f,
                   Role::help);
            if (const Button* b = byId(seeLog))
                o.button(*b, "SEE " + std::string(nameOf(h)) + "'S REP LOG", s.hover == seeLog, false);
        }
    } else {
        // REP: the meter (the filtered hero's, else the one played), then the log.
        const uint32_t h = s.filter < heroes ? s.filter : playing;
        const int32_t pts = d.heroPoints[h];
        const rep_log::Meter m = rep_log::meterOf(pts);
        o.text(std::string(nameOf(h)) + "   " + (d.reputation ? rep_log::rankText(pts) : std::string("REP OFF")), left,
               rowY0 + .0025f, filterU0 - .003f, .0056f, Role::text);
        if (const Button* b = byId(logFilter))
            o.button(*b, s.filter < heroes ? "SHOW ALL HEROES" : "ONLY " + std::string(nameOf(h)), s.hover == logFilter, false,
                     .0034f);
        o.fill(left, meterY0, right, meterY1, Role::backing, .8f);
        o.fill(left, meterY0, left + (right - left) * m.progress, meterY1, Role::accent);
        o.outline(left, meterY0, right, meterY1, Role::frame, .8f);
        o.text(rep_log::nextText(pts), left, .0445f, .030f, .0036f, Role::value);
        o.textRight(std::to_string(m.points) + " / " + std::to_string(hero072::repMost), .032f, .0445f, right, .0036f, Role::value);
        const uint32_t total = logCount(s, d);
        o.text("REP LOG", left, .0355f, -.03f, .0036f, Role::label);
        const uint32_t first = static_cast<uint32_t>(std::clamp(s.logFirst, 0.f, logMost(s, d)) + .5f);
        if (total)
            o.textRight(std::to_string(first + 1) + "-" + std::to_string(std::min(total, first + logRows)) + " OF " +
                            std::to_string(total) + " (LAST " + std::to_string(rep_log::most) + " KEPT)",
                        -.02f, .0355f, right, .003f, Role::help);
        if (!d.repLog)
            o.text("REP LOG IS OFF (HOLOGRAM: REP LOG).", left, logTop - .012f, right, .0038f, Role::help);
        else if (!total)
            o.text("NOTHING YET: DOWN A BAD GUY OR STOP A CRIME.", left, logTop - .012f, right, .0038f, Role::help);
        for (uint32_t r = 0; r < logRows; ++r) {
            const rep_log::Entry* e = d.log.nthFor(logHero(s), first + r);
            if (!e || !d.repLog)
                break;
            const float v1 = logTop - static_cast<float>(r) * logRowPitch, v0 = v1 - logRowPitch + .0008f;
            if (r % 2 == 0)
                o.fill(left, v0, right, v1, Role::backing, .45f);
            const float base = (v0 + v1) / 2 - .0016f;
            o.text(rep_log::deltaText(e->delta), left + .001f, base, left + .015f, .0032f,
                   e->delta > 0 ? Role::good : e->delta < 0 ? Role::bad : Role::help);
            o.text(rep_log::what(*e), left + .017f, base, -.014f, .0032f, Role::text);
            o.text(nameOf(e->hero), -.012f, base, .030f, .0032f, Role::label);
            o.textRight(rep_log::when(*e), .032f, base, right - .001f, .0032f, Role::value);
        }
        if (const Button* b = byId(logNewer))
            o.button(*b, "NEWER", s.hover == logNewer, false, .0036f);
        if (const Button* b = byId(logOlder))
            o.button(*b, "OLDER", s.hover == logOlder, false, .0036f);
    }
    // The help line at the foot: the hovered button's, else what to do here (a page's own, wrist_hud's, when it
    // is laid out here).
    if (hostsPage)
        return;
    const auto lines = wrap(s.settingHelp && s.hover == none ? std::string(s.settingHelp) : helpFor(s, d, s.hover), helpH,
                            right - left, 3);
    for (size_t i = 0; i < lines.size(); ++i)
        o.text(lines[i], left, helpTop - static_cast<float>(i) * helpPitch, right, helpH, Role::help);
}

Hosted hosted(const State& s) {
    if (s.screen == Screen::color)
        return Hosted::color;
    if (s.screen == Screen::hero && s.tab == Tab::music)
        return Hosted::music;
    return Hosted::none;
}
float hostedTop(const State& s) {
    switch (hosted(s)) {
    case Hosted::color: return colorTop;
    case Hosted::music: return musicTop;
    case Hosted::none: break;
    }
    return top;
}

void Holo::open(uint32_t playing) {
    const uint32_t keep = s_.presses;
    s_ = State{};
    s_.presses = keep;
    s_.screen = Screen::grid;
    s_.hero = std::min(playing, heroes - 1);
    s_.group = groupOf(s_.hero);
}
void Holo::close() {
    s_.screen = Screen::closed;
    s_.hover = none;
}
Actions Holo::update(bool hit, float u, float v, bool press, float stick, float seconds, const Data& d) {
    Actions a;
    if (s_.screen == Screen::closed)
        return a;
    if (s_.screen == Screen::soon && !d.comingSoon)
        s_.screen = Screen::grid; // COMING SOON turned off while it was shown
    const bool ok = hit && std::isfinite(u) && std::isfinite(v);
    s_.settingsPage = std::min(s_.settingsPage, std::max(1u, s_.settingsPages) - 1);
    s_.hover = ok ? buttonAt(s_, d, u, v) : none;
    const float most = logMost(s_, d);
    if (s_.screen == Screen::rep && ok && std::isfinite(stick) && std::abs(stick) > .25f && std::isfinite(seconds))
        s_.logFirst -= stick * std::clamp(seconds, 0.f, .1f) * 12.f; // stick up: newer
    s_.logFirst = std::isfinite(s_.logFirst) ? std::clamp(s_.logFirst, 0.f, most) : 0.f;
    if (!press || s_.hover == none)
        return a;
    Button bs[buttonsMost];
    const uint32_t n = buttons(s_, d, bs);
    const Button* b = nullptr;
    for (uint32_t i = 0; i < n; ++i)
        if (bs[i].id == s_.hover)
            b = &bs[i];
    if (!b || !b->enabled)
        return a;
    ++s_.presses;
    const int id = b->id;
    const uint32_t playing = std::min(d.playing, heroes - 1);
    switch (id) {
    case tabMarvel:
    case tabDc:
        s_.screen = Screen::grid;
        s_.group = id == tabDc ? 1 : 0;
        break;
    case tabSoon:
        s_.screen = Screen::soon;
        s_.soonGroup = std::min(s_.group, 1u);
        break;
    case soonMarvel:
    case soonDc: s_.soonGroup = id == soonDc ? 1 : 0; break;
    case tabRep:
        s_.screen = Screen::rep;
        s_.filter = heroes;
        s_.logFirst = 0;
        break;
    case closeButton:
        close();
        a.closed = true;
        return a;
    case backButton:
        s_.screen = Screen::grid;
        s_.group = groupOf(s_.hero);
        break;
    case playButton:
        a.playAs = static_cast<int>(std::min(s_.hero, heroes - 1));
        break;
    case subControls: s_.tab = Tab::controls; break;
    case subOutfits: s_.tab = Tab::outfits; break;
    case subStats: s_.tab = Tab::stats; break;
    case subMusic: s_.tab = Tab::music; break;
    case tabColor: s_.screen = Screen::color; break;
    case subSettings:
        s_.tab = Tab::settings;
        s_.settingsPage = 0;
        break;
    case settingsPrev: s_.settingsPage = s_.settingsPage > 0 ? s_.settingsPage - 1 : 0; break;
    case settingsNext: s_.settingsPage = std::min(s_.settingsPage + 1, std::max(1u, s_.settingsPages) - 1); break;
    case seeLog:
        s_.screen = Screen::rep;
        s_.filter = std::min(s_.hero, heroes - 1);
        s_.logFirst = 0;
        break;
    case logFilter:
        s_.filter = s_.filter < heroes ? heroes : playing;
        s_.logFirst = 0;
        break;
    case logNewer: s_.logFirst = std::max(0.f, s_.logFirst - static_cast<float>(logRows)); break;
    case logOlder: s_.logFirst = std::min(most, s_.logFirst + static_cast<float>(logRows)); break;
    default:
        if (id >= tile && id < tile + static_cast<int>(heroes)) {
            s_.screen = Screen::hero;
            s_.hero = static_cast<uint32_t>(id - tile);
            s_.settingsPage = 0;
        } else if (id >= outfit && id < outfit + static_cast<int>(roster_page::suitsMost)) {
            a.suitHero = static_cast<int>(std::min(s_.hero, heroes - 1));
            a.suitIndex = id - outfit;
        }
        break;
    }
    s_.hover = buttonAt(s_, d, u, v);
    return a;
}
} // namespace spidy::holo_roster
