// HERO VR 0.0.76 SM1: Mulder's Heroes add-on core (pure, no game memory). See include/spidy/sm1_addon.hpp.
// iilyn's Spider-Man VR mod is SpidyVR (Spidy) by Ilya Mezerowsky (GPL-3.0, https://github.com/IlyaMez/SpidyVR).
// Made by Agent Mulder.
#include "spidy/sm1_addon.hpp"

#include "spidy/hero072.hpp"
#include "spidy/pixel_font.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace spidy::sm1 {
const char* evidenceText(Evidence e) {
    return e == Evidence::confirmed ? "CONFIRMED" : "CANDIDATE";
}
const char* stateText(State s) {
    switch (s) {
    case State::ready:
        return "ready";
    case State::untested:
        return "built, not yet tried in the game";
    case State::stub:
        return "not done";
    }
    return "";
}

static char lowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}
bool sameFileName(std::string_view a, std::string_view b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (lowerAscii(a[i]) != lowerAscii(b[i]))
            return false;
    return true;
}
std::string_view fileNameOf(std::string_view path) {
    const size_t slash = path.find_last_of("\\/");
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

Verdict judge(const HostInfo& h) {
    if (!sameFileName(h.exeName, hostExe))
        return Verdict::wrongGame;
    for (bool loaded : h.spidy)
        if (!loaded)
            return Verdict::spidyMissing;
    if (!h.vr)
        return Verdict::vrNotStarted;
    if (h.version != hostVersion)
        return Verdict::otherVersion;
    return Verdict::ok;
}
bool runs(Verdict v) {
    return v == Verdict::ok || v == Verdict::otherVersion;
}
const char* verdictText(Verdict v) {
    switch (v) {
    case Verdict::ok:
        return "the game and iilyn's mod are as expected";
    case Verdict::wrongGame:
        return "this is not Spider-Man.exe; the add-on stays idle";
    case Verdict::otherVersion:
        return "the game is not version 4.630.0.0 (iilyn's supported build); the add-on runs, his mod decides the rest";
    case Verdict::spidyMissing:
        return "iilyn's modules are not all loaded; the add-on stays idle";
    case Verdict::vrNotStarted:
        return "iilyn's VR module is not loaded yet; the add-on stays idle";
    }
    return "unknown";
}

// --- Settings ----------------------------------------------------------------------------------------------------
static std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
        s.remove_suffix(1);
    return s;
}
static bool parseBool(std::string_view v, bool& out) {
    if (v == "1" || v == "true" || v == "on" || v == "yes") {
        out = true;
        return true;
    }
    if (v == "0" || v == "false" || v == "off" || v == "no") {
        out = false;
        return true;
    }
    return false;
}
static bool parseSmall(std::string_view v, unsigned long& out) {
    if (v.empty() || v.size() > 4 || !std::all_of(v.begin(), v.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
        return false;
    out = std::strtoul(std::string(v).c_str(), nullptr, 10);
    return true;
}
template <class F> static void eachLine(std::string_view text, F f) {
    while (!text.empty()) {
        const size_t nl = text.find('\n');
        std::string_view line = trim(text.substr(0, nl));
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        if (line.empty() || line.front() == '#' || line.front() == ';' || line.front() == '[')
            continue;
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos)
            continue;
        f(trim(line.substr(0, eq)), trim(line.substr(eq + 1)));
    }
}
Config parseConfig(std::string_view text) {
    Config c;
    eachLine(text, [&](std::string_view key, std::string_view value) {
        bool b = false;
        unsigned long n = 0;
        if (key == "heroes" && parseBool(value, b))
            c.heroes = b;
        else if (key == "wrist_menu" && parseBool(value, b))
            c.wristMenu = b;
        else if (key == "flight" && parseBool(value, b))
            c.flight = b;
        else if (key == "hero" && parseSmall(value, n) && n < heroCount)
            c.hero = static_cast<uint32_t>(n);
        else if (key.size() > 7 && key.substr(0, 7) == "outfit.") {
            unsigned long h = 0;
            if (parseSmall(key.substr(7), h) && h < heroCount && parseSmall(value, n) && n < outfitCount(static_cast<uint32_t>(h)))
                c.outfit[h] = static_cast<uint32_t>(n);
        }
    });
    return c;
}
std::string configText(const Config& c) {
    std::string s = "# Mulder's Heroes add-on (HERO VR) for iilyn's Spider-Man VR mod. The HERO VR launcher writes the switches,\n"
                    "# the wrist menu the hero and outfit picks.\n";
    s += std::string("heroes=") + (c.heroes ? "1" : "0") + "\n";
    s += std::string("wrist_menu=") + (c.wristMenu ? "1" : "0") + "\n";
    s += std::string("flight=") + (c.flight ? "1" : "0") + "\n";
    s += "hero=" + std::to_string(c.hero) + "\n";
    for (uint32_t h = 0; h < heroCount; ++h)
        s += "outfit." + std::to_string(h) + "=" + std::to_string(c.outfit[h]) + "\n";
    return s;
}

Live parseLive(std::string_view text) {
    Live l;
    eachLine(text, [&](std::string_view key, std::string_view value) {
        bool b = false;
        unsigned long n = 0;
        if (key == "powers" && parseBool(value, b))
            l.powers = b;
        else if (key == "hero_webs" && parseBool(value, b))
            l.heroWebs = b;
        else if (key == "beams" && parseBool(value, b))
            l.beams = b;
        else if (key == "power_damage" && parseSmall(value, n))
            l.damage = std::clamp(static_cast<int>(n), 25, 200);
        else if (key == "menu_size" && parseSmall(value, n))
            l.menuSize = std::clamp(static_cast<int>(n), 60, 160);
    });
    return l;
}
std::string liveText(const Live& l) {
    std::string s = "# Mulder's Heroes add-on (HERO VR) for iilyn's Spider-Man VR mod: the add-on's live switches.\n"
                    "# Read again every 2 seconds while the game runs.\n";
    const std::string values[] = {l.powers ? "1" : "0", l.heroWebs ? "1" : "0", l.beams ? "1" : "0",
                                  std::to_string(l.damage), std::to_string(l.menuSize)};
    int i = 0;
    for (const LiveSwitch& w : liveSwitches) {
        s += std::string("# ") + w.subtext + "\n";
        s += std::string(w.key) + "=" + values[i++] + "\n";
    }
    return s;
}

// --- Outfits -------------------------------------------------------------------------------------------------------
static const hero072::SuitCatalog& suits() {
    static const hero072::SuitCatalog catalog;
    return catalog;
}
uint32_t outfitCount(uint32_t hero) {
    return hero < heroCount ? std::max(1u, suits().suits(hero)) : 1u;
}
OutfitInfo outfitOf(uint32_t hero, uint32_t i) {
    OutfitInfo o;
    if (const hero072::Suit* x = hero < heroCount ? suits().suit(hero, i) : nullptr) {
        o.name = x->name;
        o.artist = x->artist;
        o.licence = x->licence;
    }
    return o;
}

// --- Powers --------------------------------------------------------------------------------------------------------
// Knockback numbers: his punch.hpp Knockback (0 None, 2 Stagger, 3 Knockback, 4 Knockdown, 5 FlyBack).
static constexpr Power powers[heroCount] = {
    {"WEBS (IILYN'S)", Source::none, 0, 0, 0, 0, 0xffffff},       // SPIDER-MAN: his mod as he made it
    {"REPULSOR", Source::hand, 14, 5, 30, .45f, 0x9fe8ff},        // IRON MAN
    {"BILLY CLUB", Source::hand, 9, 2, 14, .5f, 0xd02020},        // DAREDEVIL
    {"GAMMA CLAP", Source::hand, 20, 5, 6, 1.2f, 0x5cff4a},       // HULK
    {"BATARANG", Source::hand, 8, 2, 18, .4f, 0x404040},          // BATMAN
    {"CLAW LUNGE", Source::hand, 16, 3, 4, .6f, 0xd8d8e0},        // WOLVERINE
    {"HEAT VISION", Source::eyes, 12, 3, 30, .35f, 0xff3010},     // SUPERMAN
    {"PISTOL", Source::hand, 10, 2, 35, .3f, 0xffd060},           // PUNISHER
    {"SHIELD THROW", Source::hand, 16, 4, 20, .9f, 0x3060ff},     // CAPTAIN AMERICA
    {"LIGHTNING", Source::hand, 18, 4, 25, .8f, 0xb0d8ff},        // THOR
    {"RING BLAST", Source::hand, 14, 3, 28, .45f, 0x30ff50},      // GREEN LANTERN
};
const Power& powerOf(uint32_t hero) {
    return powers[hero < heroCount ? hero : 0];
}
bool aimedAt(Vec3 from, Vec3 dir, Vec3 target, float maxDegrees, float range) {
    if (!finite(from) || !finite(dir) || !finite(target) || !std::isfinite(maxDegrees) || !std::isfinite(range))
        return false;
    const Vec3 to = target - from;
    const float d = length(to), n = length(dir);
    if (!(d > 1e-3f) || !(n > 1e-6f) || d > range)
        return false;
    return dot(to, dir) / (d * n) >= std::cos(maxDegrees * 3.14159265f / 180);
}
bool flies(uint32_t hero) {
    return hero == hero_modes::ironMan || hero == hero_modes::superman || hero == hero_modes::thor ||
           hero == hero_modes::greenLantern;
}

// --- Input ---------------------------------------------------------------------------------------------------------
Binding classify(std::string_view path) {
    Binding b;
    constexpr std::string_view left = "/user/hand/left/", right = "/user/hand/right/";
    if (path.substr(0, left.size()) == left) {
        b.side = 0;
        path.remove_prefix(left.size());
    } else if (path.substr(0, right.size()) == right) {
        b.side = 1;
        path.remove_prefix(right.size());
    } else {
        return {};
    }
    if (path == "input/trigger/value" || path == "input/trigger/click" || path == "input/trigger")
        b.role = Role::trigger;
    else if (path == "input/squeeze/value" || path == "input/squeeze/click" || path == "input/squeeze/force" ||
             path == "input/squeeze")
        b.role = Role::grip;
    else if (path == "input/thumbstick" || path == "input/trackpad")
        b.role = Role::stick;
    else if (path == "input/grip/pose")
        b.role = Role::gripPose;
    else if (path == "input/aim/pose")
        b.role = Role::aimPose;
    else
        return {};
    return b;
}
Mask maskFor(const MaskInput& in) {
    Mask m;
    if (in.menuOpen) {
        m.trigger[1] = true;
        m.stick[1] = true;
    } else if (in.palmReady) {
        m.trigger[1] = true;
    }
    if (in.flightArmed)
        m.grip[0] = m.grip[1] = true;
    if (in.flying) {
        m.stick[0] = true;
        m.stickY[1] = true;
    }
    if (!in.spiderMan) {
        if (!in.heroWebs)
            m.grip[0] = m.grip[1] = true;
        if (in.powers)
            for (int i = 0; i < 2; ++i)
                if (in.handOut[i] && (!in.grip[i] || !in.heroWebs || in.flightArmed))
                    m.trigger[i] = true;
    }
    return m;
}

// --- Wrist menu ----------------------------------------------------------------------------------------------------
const char* pageName(Page p) {
    switch (p) {
    case Page::heroes:
        return "HEROES";
    case Page::outfits:
        return "OUTFITS";
    case Page::powers:
        return "POWERS";
    case Page::addon:
        return "ADD-ON";
    case Page::count:
        break;
    }
    return "";
}
const char* toggleName(Toggle t) {
    switch (t) {
    case Toggle::powers:
        return "POWERS";
    case Toggle::flight:
        return "FLIGHT";
    case Toggle::heroWebs:
        return "HERO WEBS";
    case Toggle::beams:
        return "BEAMS";
    case Toggle::count:
        break;
    }
    return "";
}
int WristMenu::rows() const {
    switch (page) {
    case Page::heroes:
        return static_cast<int>(heroCount);
    case Page::outfits:
        return static_cast<int>(std::max(1u, outfits));
    case Page::powers:
        return static_cast<int>(Toggle::count);
    case Page::addon:
    case Page::count:
        break;
    }
    return 0;
}
Pick WristMenu::step(const WristInput& in) {
    const float dt = std::isfinite(in.dt) ? std::clamp(in.dt, 0.f, .25f) : 0.f;
    const float palm = std::isfinite(in.palmUp) ? in.palmUp : 0.f;
    if (!open) {
        held = palm >= openDot ? held + dt : 0;
        if (held >= openHold) {
            open = true;
            held = 0;
            stickArmed = triggerArmed = false; // a stick or trigger already held never picks on opening
        }
        return Pick::none;
    }
    held = palm <= closeDot ? held + dt : 0;
    if (held >= closeHold) {
        open = false;
        held = 0;
        return Pick::closeMenu;
    }
    const int n = rows() + 1; // the last row goes to the next page
    if (row >= n)
        row = n - 1;
    const float stick = std::isfinite(in.stickY) ? in.stickY : 0.f;
    if (std::fabs(stick) < .3f)
        stickArmed = true;
    else if (stickArmed && std::fabs(stick) > .6f) {
        stickArmed = false;
        row = ((row + (stick > 0 ? -1 : 1)) % n + n) % n;
    }
    if (!in.trigger) {
        triggerArmed = true;
        return Pick::none;
    }
    if (!triggerArmed)
        return Pick::none;
    triggerArmed = false;
    if (row == rows()) {
        page = static_cast<Page>((static_cast<uint32_t>(page) + 1) % static_cast<uint32_t>(Page::count));
        row = 0;
        return Pick::none;
    }
    switch (page) {
    case Page::heroes:
        return Pick::hero;
    case Page::outfits:
        return Pick::outfit;
    case Page::powers:
        return Pick::toggle;
    default:
        return Pick::none;
    }
}

// --- Drawing -------------------------------------------------------------------------------------------------------
uint32_t rgba(uint32_t rgb, uint32_t alpha) {
    const uint32_t r = rgb >> 16 & 255, g = rgb >> 8 & 255, b = rgb & 255;
    return (std::min(alpha, 255u) << 24) | (b << 16) | (g << 8) | r;
}
Canvas::Canvas(int w, int h) : width(std::max(0, w)), height(std::max(0, h)), pixels(static_cast<size_t>(width) * height) {}
void Canvas::clear(uint32_t c) {
    std::fill(pixels.begin(), pixels.end(), c);
}
static uint32_t blend(uint32_t dst, uint32_t src) {
    const uint32_t a = src >> 24;
    if (a == 255)
        return src;
    if (a == 0)
        return dst;
    uint32_t out = 0;
    for (int k = 0; k < 3; ++k) {
        const uint32_t s = src >> (8 * k) & 255, d = dst >> (8 * k) & 255;
        out |= ((s * a + d * (255 - a)) / 255) << (8 * k);
    }
    const uint32_t da = dst >> 24;
    out |= (a + da * (255 - a) / 255) << 24;
    return out;
}
void Canvas::rect(int x, int y, int w, int h, uint32_t c) {
    const int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(width, x + w), y1 = std::min(height, y + h);
    for (int j = y0; j < y1; ++j)
        for (int i = x0; i < x1; ++i) {
            auto& p = pixels[static_cast<size_t>(j) * width + i];
            p = blend(p, c);
        }
}
void Canvas::frame(int x, int y, int w, int h, int t, uint32_t c) {
    rect(x, y, w, t, c);
    rect(x, y + h - t, w, t, c);
    rect(x, y + t, t, h - 2 * t, c);
    rect(x + w - t, y + t, t, h - 2 * t, c);
}
int Canvas::textWidth(std::string_view s, int scale) {
    return pixelTextColumns(s) * std::max(1, scale);
}
int Canvas::text(int x, int y, std::string_view s, int scale, uint32_t c) {
    scale = std::max(1, scale);
    int cx = x;
    for (const char ch : s) {
        int w = 0;
        const unsigned char* bits = nullptr;
        if (!pixelGlyph(ch, w, bits)) {
            cx += 4 * scale;
            continue;
        }
        for (int r = 0; r < 7; ++r)
            for (int k = 0; k < w; ++k)
                if (bits[r] >> (w - 1 - k) & 1)
                    rect(cx + k * scale, y + r * scale, scale, scale, c);
        cx += (w + 1) * scale;
    }
    return cx - x;
}
// Fits `s` into `w` pixels at the largest scale up to `scale` (at least 1); returns that scale.
static int fitScale(std::string_view s, int w, int scale) {
    while (scale > 1 && Canvas::textWidth(s, scale) > w)
        --scale;
    return scale;
}
void paintMenu(Canvas& c, const MenuView& v) {
    if (c.width != menuWidth || c.height != menuHeight)
        c = Canvas(menuWidth, menuHeight);
    const uint32_t hero = v.hero < heroCount ? v.hero : 0;
    const Power& p = powerOf(hero);
    const uint32_t accent = rgba(hero == 0 ? 0xff3030 : p.color), white = rgba(0xffffff), dim = rgba(0x9aa4b4);
    c.clear(rgba(0x0a1020, 215));
    c.frame(0, 0, c.width, c.height, 4, accent);
    // The holo card: the hero, the outfit and its artist, the power.
    c.rect(12, 12, c.width - 24, 150, rgba(0x18243c, 230));
    c.frame(12, 12, c.width - 24, 150, 2, accent);
    const char* name = heroName(hero);
    c.text(26, 24, name, fitScale(name, c.width - 52, 5), white);
    const OutfitInfo o = outfitOf(hero, v.outfit);
    const std::string outfit = "OUTFIT: " + (o.name.empty() ? std::string("DEFAULT") : o.name);
    c.text(26, 72, outfit, fitScale(outfit, c.width - 52, 3), white);
    const std::string by = o.artist.empty() ? std::string() : "BY " + o.artist + (o.licence.empty() ? "" : " (" + o.licence + ")");
    if (!by.empty())
        c.text(26, 100, by, fitScale(by, c.width - 52, 2), dim);
    std::string power = std::string(p.name);
    if (hero != 0)
        power += flies(hero) ? "  FLIES" : "";
    if (!v.powerState.empty())
        power += "  " + v.powerState;
    if (v.flying)
        power += "  FLYING";
    c.text(26, 128, power, fitScale(power, c.width - 52, 3), accent);
    // The page tabs.
    int x = 14;
    for (uint32_t i = 0; i < static_cast<uint32_t>(Page::count); ++i) {
        const auto page = static_cast<Page>(i);
        const int w = Canvas::textWidth(pageName(page), 2) + 16;
        c.rect(x, 172, w, 26, page == v.page ? accent : rgba(0x223048, 230));
        c.text(x + 8, 178, pageName(page), 2, page == v.page ? rgba(0x000000) : white);
        x += w + 6;
    }
    // The rows.
    std::vector<std::string> rows;
    switch (v.page) {
    case Page::heroes:
        for (uint32_t h = 0; h < heroCount; ++h)
            rows.push_back(std::string(h == hero ? "> " : "  ") + heroName(h));
        break;
    case Page::outfits:
        for (uint32_t i = 0; i < outfitCount(hero); ++i) {
            const OutfitInfo oi = outfitOf(hero, i);
            rows.push_back(std::string(i == v.outfit ? "> " : "  ") + (oi.name.empty() ? "DEFAULT" : oi.name));
        }
        break;
    case Page::powers:
        for (int i = 0; i < static_cast<int>(Toggle::count); ++i)
            rows.push_back(std::string("  ") + toggleName(static_cast<Toggle>(i)) + ": " + (v.toggles[i] ? "ON" : "OFF"));
        break;
    case Page::addon:
        break;
    case Page::count:
        break;
    }
    const int rowsTop = 210, rowH = 30;
    int y = rowsTop;
    for (size_t i = 0; i < rows.size(); ++i, y += rowH) {
        if (static_cast<int>(i) == v.row)
            c.rect(12, y - 3, c.width - 24, rowH - 2, rgba(0x2c4a7a, 235));
        c.text(20, y + 2, rows[i], fitScale(rows[i], c.width - 40, 3), white);
    }
    if (v.page == Page::addon) {
        for (const std::string& line : v.status) {
            if (!line.empty())
                c.text(20, y, line, fitScale(line, c.width - 40, 2), dim);
            y += 22;
        }
        y += 6;
    }
    const bool nextSel = v.row == static_cast<int>(rows.size());
    if (nextSel)
        c.rect(12, y - 3, c.width - 24, rowH - 2, rgba(0x2c4a7a, 235));
    c.text(20, y + 2, "  NEXT PAGE >", 3, nextSel ? white : dim);
    // Credits.
    c.text(20, c.height - 50, "MADE BY AGENT MULDER", 2, dim);
    c.text(20, c.height - 28, "SPIDER-MAN VR: SPIDYVR BY IILYN", 2, dim);
}
void paintBeam(Canvas& c, uint32_t rgb) {
    if (c.width <= 0 || c.height <= 0)
        c = Canvas(16, 64);
    const float half = (c.width - 1) / 2.f;
    for (int i = 0; i < c.width; ++i) {
        const float d = std::fabs(i - half) / std::max(half, 1.f); // 0 centre, 1 edge
        const float core = std::max(0.f, 1 - d * 2.2f), glow = std::max(0.f, 1 - d);
        const float a = std::min(1.f, core + glow * glow * .6f);
        uint32_t r = rgb >> 16 & 255, g = rgb >> 8 & 255, b = rgb & 255;
        r = static_cast<uint32_t>(r + (255 - r) * core);
        g = static_cast<uint32_t>(g + (255 - g) * core);
        b = static_cast<uint32_t>(b + (255 - b) * core);
        const uint32_t px = rgba(r << 16 | g << 8 | b, static_cast<uint32_t>(a * 255));
        for (int j = 0; j < c.height; ++j)
            c.pixels[static_cast<size_t>(j) * c.width + i] = px;
    }
}
bool sameView(const MenuView& a, const MenuView& b) {
    for (int i = 0; i < static_cast<int>(Toggle::count); ++i)
        if (a.toggles[i] != b.toggles[i])
            return false;
    for (int i = 0; i < 6; ++i)
        if (a.status[i] != b.status[i])
            return false;
    return a.page == b.page && a.row == b.row && a.hero == b.hero && a.outfit == b.outfit && a.flying == b.flying &&
           a.powerState == b.powerState;
}

// --- Quads ---------------------------------------------------------------------------------------------------------
Quat fromBasis(Vec3 x, Vec3 y, Vec3 z) {
    const float m00 = x.x, m11 = y.y, m22 = z.z, trace = m00 + m11 + m22;
    Quat q;
    if (trace > 0) {
        const float s = std::sqrt(trace + 1) * 2;
        q = {(y.z - z.y) / s, (z.x - x.z) / s, (x.y - y.x) / s, s / 4};
    } else if (m00 > m11 && m00 > m22) {
        const float s = std::sqrt(1 + m00 - m11 - m22) * 2;
        q = {s / 4, (y.x + x.y) / s, (z.x + x.z) / s, (y.z - z.y) / s};
    } else if (m11 > m22) {
        const float s = std::sqrt(1 + m11 - m00 - m22) * 2;
        q = {(y.x + x.y) / s, s / 4, (z.y + y.z) / s, (z.x - x.z) / s};
    } else {
        const float s = std::sqrt(1 + m22 - m00 - m11) * 2;
        q = {(z.x + x.z) / s, (z.y + y.z) / s, s / 4, (x.y - y.x) / s};
    }
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return n > 1e-6f ? Quat{q.x / n, q.y / n, q.z / n, q.w / n} : Quat{};
}
float palmUp(Quat leftGrip) {
    return dot(leftGrip.rotate({1, 0, 0}), {0, 1, 0});
}
Pose menuPose(Pose leftGrip, Vec3 head, float lift) {
    Pose p;
    p.position = leftGrip.position + Vec3{0, lift, 0};
    Vec3 toHead = head - p.position;
    toHead.y = 0;
    Vec3 z = normalized(toHead);
    if (length(z) < .5f)
        z = {0, 0, 1};
    const Vec3 y{0, 1, 0};
    const Vec3 x = normalized(cross(y, z));
    p.orientation = fromBasis(x, y, z);
    return p;
}
Pose beamPose(Vec3 from, Vec3 dir, float len, Vec3 head) {
    Pose p;
    const Vec3 y = normalized(dir);
    p.position = from + y * (len / 2);
    Vec3 toHead = head - p.position;
    Vec3 z = normalized(toHead - y * dot(toHead, y));
    if (length(z) < .5f)
        z = normalized(cross(y, std::fabs(y.y) < .9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0}));
    const Vec3 x = normalized(cross(y, z));
    p.orientation = fromBasis(x, y, z);
    return p;
}

// --- Flight --------------------------------------------------------------------------------------------------------
FlightEvent stepFlight(FlightState& s, const FlightInput& in, const FlightTuning& t) {
    const float dt = std::isfinite(in.dt) ? std::clamp(in.dt, 0.f, .1f) : 0.f;
    if (!finite(s.velocity))
        s.velocity = {};
    const auto axis = [](float v) { return std::isfinite(v) ? std::clamp(v, -1.f, 1.f) : 0.f; };
    if (!s.on) {
        s.velocity = {};
        const bool grip = in.grip[0] || in.grip[1];
        s.gripHeld = in.allowed && grip ? s.gripHeld + dt : 0;
        if (s.gripHeld >= t.enterHold) {
            s.on = true;
            s.gripHeld = 0;
            s.airTime = 0;
            s.gripArmed[0] = !in.grip[0];
            s.gripArmed[1] = !in.grip[1];
            s.velocity = {0, t.liftOff, 0};
            return FlightEvent::tookOff;
        }
        return FlightEvent::none;
    }
    if (!in.allowed || in.stop) {
        s = {};
        return FlightEvent::stopped;
    }
    s.airTime += dt;
    Vec3 forward{in.headForward.x, 0, in.headForward.z}, right{in.headRight.x, 0, in.headRight.z};
    forward = normalized(finite(forward) ? forward : Vec3{});
    right = normalized(finite(right) ? right : Vec3{});
    Vec3 goal = forward * (axis(in.moveY) * t.cruise) + right * (axis(in.moveX) * t.cruise) +
                Vec3{0, axis(in.climb) * t.climb, 0};
    for (int i = 0; i < 2; ++i) {
        if (!in.grip[i])
            s.gripArmed[i] = true;
        else if (s.gripArmed[i] && finite(in.aim[i]))
            goal += normalized(in.aim[i]) * t.boost;
    }
    goal = limited(goal, t.maxSpeed);
    s.velocity += (goal - s.velocity) * std::min(1.f, t.response * dt);
    s.velocity = limited(s.velocity, t.maxSpeed);
    if (s.airTime >= t.landAfter && in.grounded && s.velocity.y <= .5f && axis(in.climb) <= 0) {
        s = {};
        return FlightEvent::landed;
    }
    return FlightEvent::none;
}

// --- Hand-over -----------------------------------------------------------------------------------------------------
uint64_t Handover::his(uint64_t serial) {
    if (ours_)
        return 0;
    uint64_t out = serial + offset_;
    if (out <= last_) {
        offset_ = last_ + 1 - serial;
        out = last_ + 1;
    }
    last_ = out;
    ring_[next_++ % 256] = {out, serial};
    return out;
}
uint64_t Handover::ours() {
    used_ = true;
    last_ = std::max(last_ + 1, base);
    return last_;
}
void Handover::refused() {
    last_ += 1ull << 20;
}
uint64_t Handover::sampleSerial(uint64_t shown) const {
    if (!shown)
        return 0;
    if (!used_ && offset_ == 0)
        return shown; // nothing of ours went out yet: his serials pass as they are
    for (const Pair& p : ring_)
        if (p.out == shown && p.out)
            return p.in;
    return 0;
}
} // namespace spidy::sm1
