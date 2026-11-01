// HERO VR 0.0.76 SM1: Mulder's Heroes add-on core (pure, no game memory). See include/spidy/sm1_addon.hpp.
// iilyn's Spider-Man VR mod is SpidyVR (Spidy) by Ilya Mezerowsky (GPL-3.0, https://github.com/IlyaMez/SpidyVR).
// Made by Agent Mulder.
#include "spidy/sm1_addon.hpp"

#include "spidy/hero072.hpp"
#include "spidy/pixel_font.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

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
static bool parseSigned(std::string_view v, long& out) {
    const bool minus = !v.empty() && v.front() == '-';
    unsigned long n = 0;
    std::string_view d = minus ? v.substr(1) : v;
    if (d.empty() || d.size() > 4 || !std::all_of(d.begin(), d.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
        return false;
    n = std::strtoul(std::string(d).c_str(), nullptr, 10);
    out = minus ? -static_cast<long>(n) : static_cast<long>(n);
    return true;
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
        long sn = 0;
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
        else if (key == "sm1scan" && parseBool(value, b))
            l.scan = b;
        else if (key == "grapple" && parseBool(value, b))
            l.grapple = b;
        else if (key == "color_fx" && parseBool(value, b))
            l.colorFx = b;
        else if (key == "color_filter") {
            if (parseSmall(value, n) && n < eye_color::filters)
                l.colorFilter = static_cast<uint32_t>(n);
            else
                for (uint32_t f = 0; f < eye_color::filters; ++f)
                    if (value == eye_color::filterKeys[f])
                        l.colorFilter = f;
        } else if (key == "color_strength" && parseSmall(value, n))
            l.colorStrength = std::min<uint32_t>(static_cast<uint32_t>(n), 100);
        else if (key == "color_exposure" && parseSigned(value, sn))
            l.colorExposure = static_cast<int32_t>(std::clamp(sn, -20l, 20l));
        else if (key == "color_contrast" && parseSmall(value, n))
            l.colorContrast = std::clamp<uint32_t>(static_cast<uint32_t>(n), 50, 150);
        else if (key == "color_saturation" && parseSmall(value, n))
            l.colorSaturation = std::min<uint32_t>(static_cast<uint32_t>(n), 200);
        else if (key == "color_vibrance" && parseSmall(value, n))
            l.colorVibrance = std::min<uint32_t>(static_cast<uint32_t>(n), 100);
        else if (key == "color_warmth" && parseSigned(value, sn))
            l.colorWarmth = static_cast<int32_t>(std::clamp(sn, -100l, 100l));
        else if (key == "color_tint" && parseSigned(value, sn))
            l.colorTint = static_cast<int32_t>(std::clamp(sn, -100l, 100l));
        else if (key == "color_vignette" && parseSmall(value, n))
            l.colorVignette = std::min<uint32_t>(static_cast<uint32_t>(n), 100);
        else if (key == "props" && parseBool(value, b))
            l.props = b;
    });
    return l;
}
std::string liveText(const Live& l) {
    std::string s = "# Mulder's Heroes add-on (HERO VR) for iilyn's Spider-Man VR mod: the add-on's live switches.\n"
                    "# Read again every 2 seconds while the game runs.\n";
    const std::string values[] = {l.powers ? "1" : "0", l.heroWebs ? "1" : "0", l.beams ? "1" : "0",
                                  std::to_string(l.damage), std::to_string(l.menuSize), l.scan ? "1" : "0",
                                  l.grapple ? "1" : "0", l.colorFx ? "1" : "0",
                                  eye_color::filterKeys[l.colorFilter < eye_color::filters ? l.colorFilter : 0],
                                  std::to_string(l.colorStrength), std::to_string(l.colorExposure),
                                  std::to_string(l.colorContrast), std::to_string(l.colorSaturation),
                                  std::to_string(l.colorVibrance), std::to_string(l.colorWarmth),
                                  std::to_string(l.colorTint), std::to_string(l.colorVignette),
                                  l.props ? "1" : "0"};
    static_assert(sizeof(values) / sizeof(values[0]) == sizeof(liveSwitches) / sizeof(liveSwitches[0]),
                  "one value per live switch");
    int i = 0;
    for (const LiveSwitch& w : liveSwitches) {
        s += std::string("# ") + w.subtext + "\n";
        s += std::string(w.key) + "=" + values[i++] + "\n";
    }
    return s;
}

eye_color::Config colorConfig(const Live& l) {
    eye_color::Config c;
    c.on = l.colorFx;
    c.filter = l.colorFilter < eye_color::filters ? l.colorFilter : 0;
    c.strength = l.colorStrength;
    c.exposureTenths = l.colorExposure;
    c.contrast = l.colorContrast;
    c.saturation = l.colorSaturation;
    c.vibrance = l.colorVibrance;
    c.warmth = l.colorWarmth;
    c.tint = l.colorTint;
    c.vignette = l.colorVignette;
    return c;
}
const char* colorRowName(int row) {
    static constexpr const char* names[colorRows] = {"COLOR",    "FILTER",     "STRENGTH", "EXPOSURE", "CONTRAST",
                                                     "SATURATION", "VIBRANCE", "WARMTH",   "TINT",     "VIGNETTE"};
    return row >= 0 && row < colorRows ? names[row] : "";
}
const char* colorRowSubtext(int row) {
    static constexpr const char* keys[colorRows] = {"color_fx",         "color_filter",   "color_strength", "color_exposure",
                                                    "color_contrast",   "color_saturation", "color_vibrance", "color_warmth",
                                                    "color_tint",       "color_vignette"};
    if (row < 0 || row >= colorRows)
        return "";
    for (const LiveSwitch& w : liveSwitches)
        if (std::string_view(w.key) == keys[row])
            return w.subtext;
    return "";
}
namespace {
struct ColorSlider {
    eye_color::Limits limits;
    int Live::*signedField;
    uint32_t Live::*plainField;
};
// rows 2..9
const ColorSlider& sliderOf(int row) {
    static const ColorSlider sliders[] = {
        {eye_color::strengthLimits, nullptr, &Live::colorStrength},
        {eye_color::exposureLimits, &Live::colorExposure, nullptr},
        {eye_color::contrastLimits, nullptr, &Live::colorContrast},
        {eye_color::saturationLimits, nullptr, &Live::colorSaturation},
        {eye_color::vibranceLimits, nullptr, &Live::colorVibrance},
        {eye_color::warmthLimits, &Live::colorWarmth, nullptr},
        {eye_color::tintLimits, &Live::colorTint, nullptr},
        {eye_color::vignetteLimits, nullptr, &Live::colorVignette},
    };
    return sliders[std::clamp(row - 2, 0, 7)];
}
} // namespace
std::string colorRowValue(const Live& l, int row) {
    if (row == 0)
        return l.colorFx ? "ON" : "OFF";
    if (row == 1)
        return eye_color::filterName(l.colorFilter);
    if (row < 2 || row >= colorRows)
        return "";
    const ColorSlider& s = sliderOf(row);
    const int v = s.signedField ? l.*s.signedField : static_cast<int>(l.*s.plainField);
    if (row == 3) { // exposure in stops
        const int a = v < 0 ? -v : v;
        return std::string(v < 0 ? "-" : v > 0 ? "+" : "") + std::to_string(a / 10) + "." + std::to_string(a % 10) + " EV";
    }
    return (s.signedField && v > 0 ? "+" : "") + std::to_string(v);
}
void adjustColor(Live& l, int row, int delta) {
    const int d = delta < 0 ? -1 : 1;
    if (row == 0) {
        l.colorFx = !l.colorFx;
        return;
    }
    if (row == 1) {
        const int n = static_cast<int>(eye_color::filters);
        l.colorFilter = static_cast<uint32_t>(((static_cast<int>(l.colorFilter) + d) % n + n) % n);
        return;
    }
    if (row < 2 || row >= colorRows)
        return;
    const ColorSlider& s = sliderOf(row);
    int v = s.signedField ? l.*s.signedField : static_cast<int>(l.*s.plainField);
    v += d * s.limits.step;
    if (v > s.limits.hi)
        v = s.limits.lo;
    else if (v < s.limits.lo)
        v = s.limits.hi;
    if (s.signedField)
        l.*s.signedField = v;
    else
        l.*s.plainField = static_cast<uint32_t>(v);
}

std::string missingLive(std::string_view text) {
    std::vector<std::string> keys;
    eachLine(text, [&](std::string_view key, std::string_view) { keys.emplace_back(key); });
    const std::string defaults = liveText(Live{});
    std::string add;
    for (const LiveSwitch& w : liveSwitches) {
        if (std::find(keys.begin(), keys.end(), w.key) != keys.end())
            continue;
        const std::string want = std::string(w.key) + "=";
        const size_t at = defaults.find("\n" + want);
        const size_t end = at == std::string::npos ? at : defaults.find('\n', at + 1);
        add += std::string("# ") + w.subtext + "\n";
        add += at == std::string::npos ? want + "1\n" : defaults.substr(at + 1, end - at);
    }
    return add;
}

// --- Hero suits ----------------------------------------------------------------------------------------------------
namespace {
struct SuitEntry {
    const char* id;
    const char* name;
};
constexpr SuitEntry suitTable[] = {
    {nullptr, ""},
    {"herovr_ironman", "Iron Man"},
    {"herovr_daredevil", "Daredevil"},
    {"herovr_hulk", "Hulk"},
    {"herovr_batman", "Batman"},
    {"herovr_wolverine", "Wolverine"},
    {"herovr_superman", "Superman"},
    {"herovr_punisher", "Punisher"},
    {"herovr_captainamerica", "Captain America"},
    {"herovr_thor", "Thor"},
    {"herovr_greenlantern", "Green Lantern"},
};
static_assert(sizeof(suitTable) / sizeof(suitTable[0]) == heroCount, "one suit row per hero");
} // namespace

const char* suitIdOf(uint32_t hero) { return hero < heroCount ? suitTable[hero].id : nullptr; }
const char* suitNameOf(uint32_t hero) { return hero < heroCount ? suitTable[hero].name : ""; }

bool suitInstalled(std::string_view json, uint32_t hero) {
    const char* id = suitIdOf(hero);
    if (!id)
        return false;
    // sm1suits.py writes json.dump's default separators; a half-done install keeps "pending": true
    if (json.find("\"pending\": false") == std::string_view::npos)
        return false;
    const size_t at = json.find("\"suits\": [");
    if (at == std::string_view::npos)
        return false;
    const size_t end = json.find(']', at);
    const std::string quoted = std::string("\"") + id + "\"";
    const size_t hit = json.find(quoted, at);
    return hit != std::string_view::npos && hit < end;
}

std::string suitHint(uint32_t hero, bool installed) {
    if (!suitIdOf(hero))
        return "SUIT: YOUR SPIDER-MAN SUIT (GAME'S SUITS MENU)";
    std::string name = suitNameOf(hero);
    for (auto& ch : name)
        ch = static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - 32 : ch);
    if (!installed)
        return "SUIT: INSTALL HERO SUITS IN THE LAUNCHER";
    return "SUIT: PICK " + name + " IN THE GAME'S SUITS MENU";
}

std::string_view folderOf(std::string_view path) {
    const size_t i = path.find_last_of("\\/");
    return i == std::string_view::npos ? std::string_view() : path.substr(0, i + 1);
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
    // His A on the right (his "jump", CONFIRMED in his 0.7.0 controller_layout.hpp), X on the left (Index: the left
    // a/click). The Steam Frame has X and Y on the right too: those are not A and B, so only the left x / y count.
    else if (path == "input/a/click" || (path == "input/x/click" && b.side == 0))
        b.role = Role::faceLower;
    else if (path == "input/b/click" || (path == "input/y/click" && b.side == 0))
        b.role = Role::faceUpper;
    else
        return {};
    return b;
}
Mask maskFor(const MaskInput& in) {
    Mask m;
    if (in.menuOpen) {
        m.trigger[1] = true;
        m.stick[1] = true;
    }
    for (int i = 0; i < 2; ++i)
        if (in.holoHands >> i & 1u)
            m.trigger[i] = m.grip[i] = true; // the SM2 wrist click's pull: never a web, punch or power
    if (in.flying) {
        m.stick[0] = true;                  // the speed
        m.faceLower[1] = m.faceUpper[1] = true; // A comes down, B brakes
    }
    if (in.aHeld)
        m.faceLower[1] = true; // an A held in flight is never also his jump, even once landed
    for (int i = 0; i < 2; ++i)
        if (in.triggerOurs[i])
            m.trigger[i] = true;
    if (in.grappleArmed)
        for (int i = 0; i < 2; ++i) {
            m.grip[i] = true;
            if (in.grip[i])
                m.trigger[i] = true;
        }
    if (!in.spiderMan) {
        if (!in.heroWebs)
            m.grip[0] = m.grip[1] = true;
        if (in.powers)
            for (int i = 0; i < 2; ++i)
                if (in.handOut[i] && (!in.grip[i] || !in.heroWebs))
                    m.trigger[i] = true;
    }
    return m;
}
bool keepTrigger(float trigger, bool hiddenNow) {
    return hiddenNow && std::isfinite(trigger) && trigger > .1f;
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
        return static_cast<int>(heroCount) + colorRows; // the heroes, then the COLOR rows
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
int WristMenu::colorRow() const {
    const int k = row - static_cast<int>(heroCount);
    return page == Page::heroes && k >= 0 && k < colorRows ? k : -1;
}
Pick WristMenu::step(const WristInput& in) {
    if (in.toggled) {
        open = !open;
        stickArmed = triggerArmed = false; // the click's own pull (and a stick already pushed) never picks
        return open ? Pick::none : Pick::closeMenu;
    }
    if (!open)
        return Pick::none;
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
    // A COLOR row (on the HEROES page): a left / right flick steps the row's value down / up.
    const float sx = std::isfinite(in.stickX) ? in.stickX : 0.f;
    if (std::fabs(sx) < .3f)
        stickXArmed = true;
    else if (stickXArmed && std::fabs(sx) > .6f) {
        stickXArmed = false;
        if (colorRow() >= 0) {
            delta = sx > 0 ? 1 : -1;
            return Pick::color;
        }
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
        if (colorRow() >= 0) {
            delta = 1;
            return Pick::color;
        }
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
// The SM2 build's hologram look (wrist_hud.cpp: its Spider-Man palette, palettes[0], in sRGB): translucent dark
// glass with cut corners, a red frame with a soft glow, a thin electric-blue inner line, ticks in the corners and
// the spider on a dark disc on the top edge; blue lines and pale blue-white text.
namespace holo {
constexpr uint32_t frame = 0xff473f, accent = 0x59bcff, line = 0xbcefff, text = 0xeffaff, help = 0xcee0f1,
                   backing = 0x0d223f;
constexpr uint32_t backingAlpha = 148; // SM2's .58
constexpr int cut = 30, top = 18;      // the corner cut, and the panel's top edge (the spider sits on it)
} // namespace holo
static void plot(Canvas& c, float x0, float y0, float x1, float y1, float t, uint32_t col) {
    const float dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
    const int n = std::max(1, static_cast<int>(len));
    const int r = std::max(1, static_cast<int>(t));
    // premultiplied, as blend() leaves the canvas (the quad layer is SOURCE_ALPHA, premultiplied)
    const uint32_t a = col >> 24;
    col = (a << 24) | ((col >> 16 & 255) * a / 255) << 16 | ((col >> 8 & 255) * a / 255) << 8 | (col & 255) * a / 255;
    for (int i = 0; i <= n; ++i) {
        const float k = static_cast<float>(i) / static_cast<float>(n);
        const int x = static_cast<int>(x0 + dx * k), y = static_cast<int>(y0 + dy * k);
        for (int j = y - r / 2; j < y - r / 2 + r; ++j)
            for (int q = x - r / 2; q < x - r / 2 + r; ++q)
                if (q >= 0 && j >= 0 && q < c.width && j < c.height) {
                    auto& px = c.pixels[static_cast<size_t>(j) * c.width + q];
                    // max per channel: overlapping stamps never build up past the colour
                    uint32_t out = 0;
                    for (int b = 0; b < 4; ++b)
                        out |= std::max(px >> (8 * b) & 255, col >> (8 * b) & 255) << (8 * b);
                    px = out;
                }
    }
}
static void disc(Canvas& c, int cx, int cy, int r0, int r1, uint32_t col) {
    for (int j = cy - r1; j <= cy + r1; ++j)
        for (int i = cx - r1; i <= cx + r1; ++i) {
            const int d2 = (i - cx) * (i - cx) + (j - cy) * (j - cy);
            if (d2 <= r1 * r1 && d2 >= r0 * r0)
                c.rect(i, j, 1, 1, col);
        }
}
static void holoPanel(Canvas& c) {
    using namespace holo;
    c.clear(0); // transparent outside the panel
    const int w = c.width, h = c.height, y0 = top, y1 = h - 1;
    // the glass, cut at the four corners
    for (int y = y0; y <= y1; ++y) {
        const int in = std::max({0, cut - (y - y0), cut - (y1 - y)});
        c.rect(in, y, w - 2 * in, 1, rgba(backing, backingAlpha));
    }
    const float a = 1, b = static_cast<float>(w - 2);
    const float pts[8][2] = {{a + cut, float(y0)}, {b - cut, float(y0)}, {b, float(y0 + cut)}, {b, float(y1 - cut)},
                             {b - cut, float(y1)}, {a + cut, float(y1)}, {a, float(y1 - cut)}, {a, float(y0 + cut)}};
    for (int i = 0; i < 8; ++i) { // the glow, then the bright frame
        const auto& p = pts[i];
        const auto& q = pts[(i + 1) % 8];
        plot(c, p[0], p[1], q[0], q[1], 7, rgba(frame, 40));
    }
    for (int i = 0; i < 8; ++i) {
        const auto& p = pts[i];
        const auto& q = pts[(i + 1) % 8];
        plot(c, p[0], p[1], q[0], q[1], 2, rgba(frame, 242));
    }
    const float m = 7; // the inner accent line
    const float ip[8][2] = {{a + cut + m * .4f, y0 + m}, {b - cut - m * .4f, y0 + m}, {b - m, y0 + cut + m * .4f},
                            {b - m, y1 - cut - m * .4f}, {b - cut - m * .4f, y1 - m}, {a + cut + m * .4f, y1 - m},
                            {a + m, y1 - cut - m * .4f}, {a + m, y0 + cut + m * .4f}};
    for (int i = 0; i < 8; ++i)
        plot(c, ip[i][0], ip[i][1], ip[(i + 1) % 8][0], ip[(i + 1) % 8][1], 1, rgba(accent, 90));
    for (int i = 1; i < 8; i += 2) // a tick inside each cut corner
        plot(c, (pts[i][0] + pts[(i + 1) % 8][0]) / 2 + (i == 1 || i == 3 ? -8.f : 8.f),
             (pts[i][1] + pts[(i + 1) % 8][1]) / 2 + (i == 1 || i == 7 ? 8.f : -8.f),
             (pts[i][0] + pts[(i + 1) % 8][0]) / 2 + (i == 1 || i == 3 ? -14.f : 14.f),
             (pts[i][1] + pts[(i + 1) % 8][1]) / 2 + (i == 1 || i == 7 ? 14.f : -14.f), 2, rgba(frame, 190));
    // the spider on the top edge, on a dark disc
    const int cx = w / 2, cy = y0;
    disc(c, cx, cy, 0, 16, rgba(backing, 242));
    disc(c, cx, cy, 14, 16, rgba(frame, 242));
    disc(c, cx, cy - 4, 0, 3, rgba(frame, 255)); // head
    for (int j = 0; j < 9; ++j)                  // body
        c.rect(cx - 3 + (j < 2 || j > 6 ? 1 : 0), cy - 1 + j, 6 - (j < 2 || j > 6 ? 2 : 0), 1, rgba(frame, 255));
    const float fx = static_cast<float>(cx), fy = static_cast<float>(cy);
    const float legs[4][4] = {{3, -2, 10, -9}, {3, 1, 11, -2}, {3, 4, 11, 6}, {3, 6, 9, 11}};
    for (const auto& l : legs)
        for (const float side : {-1.f, 1.f})
            plot(c, fx + side * l[0], fy + l[1], fx + side * l[2], fy + l[3], 1, rgba(frame, 255));
}
void paintMenu(Canvas& c, const MenuView& v) {
    if (c.width != menuWidth || c.height != menuHeight)
        c = Canvas(menuWidth, menuHeight);
    const uint32_t hero = v.hero < heroCount ? v.hero : 0;
    const Power& p = powerOf(hero);
    const uint32_t accent = rgba(holo::accent), white = rgba(holo::text), dim = rgba(holo::help),
                   heroColour = rgba(hero == 0 ? holo::frame : p.color);
    holoPanel(c);
    // The holo card: the hero, the outfit and its artist, the power.
    c.rect(18, 34, c.width - 36, 146, rgba(holo::accent, 26));
    c.frame(18, 34, c.width - 36, 146, 1, rgba(holo::accent, 150));
    const char* name = heroName(hero);
    c.text(30, 44, name, fitScale(name, c.width - 60, 5), white);
    const OutfitInfo o = outfitOf(hero, v.outfit);
    const std::string outfit = "OUTFIT: " + (o.name.empty() ? std::string("DEFAULT") : o.name);
    c.text(30, 92, outfit, fitScale(outfit, c.width - 60, 3), rgba(holo::line));
    const std::string by = o.artist.empty() ? std::string() : "BY " + o.artist + (o.licence.empty() ? "" : " (" + o.licence + ")");
    if (!v.notice.empty())
        c.text(30, 120, v.notice, fitScale(v.notice, c.width - 60, 2), rgba(holo::accent));
    else if (!by.empty())
        c.text(30, 120, by, fitScale(by, c.width - 60, 2), dim);
    std::string power = std::string(p.name);
    if (hero != 0)
        power += flies(hero) ? "  FLIES" : "";
    if (!v.powerState.empty())
        power += "  " + v.powerState;
    if (v.flying)
        power += "  FLYING";
    c.text(30, 148, power, fitScale(power, c.width - 60, 3), heroColour);
    // The page tabs.
    int x = 20;
    for (uint32_t i = 0; i < static_cast<uint32_t>(Page::count); ++i) {
        const auto page = static_cast<Page>(i);
        const int w = Canvas::textWidth(pageName(page), 2) + 16;
        c.rect(x, 190, w, 26, page == v.page ? accent : rgba(holo::accent, 40));
        c.text(x + 8, 196, pageName(page), 2, page == v.page ? rgba(0x000000) : white);
        x += w + 6;
    }
    // The rows.
    std::vector<std::string> rows;
    int y = 228;
    const int rowH = 28;
    const auto selected = [&](int x0, int y0, int w, int h) {
        c.rect(x0, y0 - 3, w, h - 2, rgba(holo::accent, 70));
        c.rect(x0, y0 - 3, 3, h - 2, accent);
    };
    switch (v.page) {
    case Page::heroes: {
        // 0.0.76: the heroes and the COLOR rows on one page, each in two columns (down the left, then the right),
        // then the selected COLOR row's subtext.
        const int colW = (c.width - 40) / 2 - 3, pitch = 22;
        const auto grid = [&](int first, int count, int top, auto label) {
            const int perCol = (count + 1) / 2;
            for (int i = 0; i < count; ++i) {
                const int x0 = 20 + (i / perCol) * (colW + 6), y0 = top + (i % perCol) * pitch;
                if (first + i == v.row)
                    selected(x0, y0, colW, pitch);
                const std::string t = label(i);
                c.text(x0 + 6, y0 + 1, t, fitScale(t, colW - 10, 2), white);
            }
            return top + perCol * pitch;
        };
        y = grid(0, static_cast<int>(heroCount), y, [&](int h) {
            return std::string(static_cast<uint32_t>(h) == hero ? "> " : "  ") + heroName(static_cast<uint32_t>(h));
        });
        c.rect(20, y + 3, c.width - 40, 1, rgba(holo::accent, 120));
        c.text(20, y + 7, "COLOR", 2, accent);
        y = grid(static_cast<int>(heroCount), colorRows, y + 24, [&](int i) {
            return std::string(colorRowName(i)) + ": " + v.color[i];
        });
        const int k = v.row - static_cast<int>(heroCount);
        std::string sub = k >= 0 && k < colorRows ? colorRowSubtext(k)
                          : v.row >= 0 && v.row < static_cast<int>(heroCount)
                              ? std::string("TRIGGER: PLAY AS ") + heroName(static_cast<uint32_t>(v.row))
                              : std::string();
        for (auto& ch : sub)
            ch = static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - 32 : ch);
        // Wrapped at spaces into up to three lines.
        std::vector<std::string> lines;
        std::string line;
        size_t i = 0;
        while (i <= sub.size()) {
            size_t sp = sub.find(' ', i);
            if (sp == std::string::npos)
                sp = sub.size();
            const std::string word = sub.substr(i, sp - i);
            const std::string tryLine = line.empty() ? word : line + " " + word;
            if (!line.empty() && Canvas::textWidth(tryLine, 2) > c.width - 40) {
                lines.push_back(line);
                line = word;
            } else {
                line = tryLine;
            }
            i = sp + 1;
        }
        if (!line.empty())
            lines.push_back(line);
        for (size_t l = 0; l < lines.size() && l < 3; ++l)
            c.text(20, y + 4 + static_cast<int>(l) * 16, lines[l], fitScale(lines[l], c.width - 40, 2), dim);
        y += 4 + 3 * 16 + 6;
        const bool nextSel = v.row == static_cast<int>(heroCount) + colorRows;
        if (nextSel)
            selected(20, y, c.width - 40, rowH);
        c.text(28, y + 2, "  NEXT PAGE >", 3, nextSel ? white : dim);
        c.text(30, c.height - 54, "MADE BY AGENT MULDER", 2, dim);
        c.text(30, c.height - 32, "SPIDER-MAN VR: SPIDYVR BY IILYN", 2, dim);
        return;
    }
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
    for (size_t i = 0; i < rows.size(); ++i, y += rowH) {
        if (static_cast<int>(i) == v.row)
            selected(20, y, c.width - 40, rowH);
        c.text(28, y + 2, rows[i], fitScale(rows[i], c.width - 56, 3), white);
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
        selected(20, y, c.width - 40, rowH);
    c.text(28, y + 2, "  NEXT PAGE >", 3, nextSel ? white : dim);
    // Credits.
    c.text(30, c.height - 54, "MADE BY AGENT MULDER", 2, dim);
    c.text(30, c.height - 32, "SPIDER-MAN VR: SPIDYVR BY IILYN", 2, dim);
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
    for (int i = 0; i < colorRows; ++i)
        if (a.color[i] != b.color[i])
            return false;
    return a.page == b.page && a.row == b.row && a.hero == b.hero && a.outfit == b.outfit && a.flying == b.flying &&
           a.powerState == b.powerState && a.notice == b.notice;
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
Vec3 xrDataForward(const float* m) {
    return normalized(Vec3{m[8], m[9], m[10]});
}
Vec3 gameToHeadLocal(Vec3 dir, Vec3 headRight, Vec3 headDown, Vec3 headForward) {
    return {dot(dir, headRight), -dot(dir, headDown), -dot(dir, headForward)};
}

// --- Flight --------------------------------------------------------------------------------------------------------
FlightEvent stepFlight(FlightState& s, const FlightInput& in, const FlightTuning& t) {
    const float dt = fly076::seconds(in.dt);
    if (!finite(s.velocity))
        s.velocity = {};
    const bool live = in.allowed && !in.stop;
    if (s.on && !live) {
        s = {};
        s.start.reset(); // the A still down never takes off again by itself
        return FlightEvent::stopped;
    }
    const bool enter = s.start.update(dt, live, s.on, in.armsUp, in.a);
    const float cruise = std::isfinite(t.cruise) && t.cruise > 1 ? t.cruise : 16.f;
    const float most = std::isfinite(t.maxSpeed) && t.maxSpeed > cruise ? t.maxSpeed : cruise;
    fly076::In fi;
    fi.seconds = dt;
    fi.speed = live ? 2u : 0u;
    fi.top = cruise;
    fi.grounded = in.grounded;
    fi.airborne = !in.grounded;
    fi.enter = enter;
    fi.drop = s.start.descend();
    fi.brake = s.on && in.b;
    fi.throttle = fly076::axis(in.throttle);
    fi.boost = s.sonic.update(s.on, in.throttle) ? most / cruise : 1.f;
    fi.sonicTop = most;
    fi.look = finite(in.headForward) ? in.headForward : Vec3{0, 0, 1};
    fi.want = finite(in.steer) ? in.steer : Vec3{};
    fi.velocity = s.velocity;
    const fly076::Out o = s.flight.update(fi);
    s.how = o.how;
    if (o.tookOff && !s.on) {
        s.on = true;
        s.velocity = finite(o.velocity) ? limited(o.velocity, most) : Vec3{};
        return FlightEvent::tookOff;
    }
    if (o.landed || (s.on && !s.flight.flying())) {
        const bool landed = o.landed;
        s.on = false;
        s.velocity = {};
        s.flight.land();
        return landed ? FlightEvent::landed : FlightEvent::stopped;
    }
    s.velocity = s.on && o.drive && finite(o.velocity) ? limited(o.velocity, most) : Vec3{};
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
