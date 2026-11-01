// HERO VR 0.0.76 SM1: the add-on's hologram, the SM2 build's own panel drawn into a quad's picture (sm1_holo.hpp).
// Made by Agent Mulder.
#include "spidy/sm1_holo.hpp"

#include "spidy/eye_color.hpp"
#include "spidy/hero072.hpp"
#include "spidy/hero_modes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace spidy::sm1::holo {
namespace {
// sRGB <-> linear (the hologram's colours are linear, the picture's bytes sRGB).
float toLinear(float c) {
    return c <= .04045f ? c / 12.92f : std::pow((c + .055f) / 1.055f, 2.4f);
}
const std::array<float, 256>& linearOfByte() {
    static const std::array<float, 256> t = [] {
        std::array<float, 256> a{};
        for (int i = 0; i < 256; ++i)
            a[static_cast<size_t>(i)] = toLinear(static_cast<float>(i) / 255.f);
        return a;
    }();
    return t;
}
const std::array<uint8_t, 4096>& byteOfLinear() {
    static const std::array<uint8_t, 4096> t = [] {
        std::array<uint8_t, 4096> a{};
        for (int i = 0; i < 4096; ++i) {
            const float l = static_cast<float>(i) / 4095.f;
            const float s = l <= .0031308f ? l * 12.92f : 1.055f * std::pow(l, 1 / 2.4f) - .055f;
            a[static_cast<size_t>(i)] = static_cast<uint8_t>(std::clamp(std::lround(s * 255.f), 0l, 255l));
        }
        return a;
    }();
    return t;
}
uint8_t encode(float linear) {
    const float c = std::isfinite(linear) ? std::clamp(linear, 0.f, 1.f) : 0.f;
    return byteOfLinear()[static_cast<size_t>(std::lround(c * 4095.f))];
}

struct Target {
    int w, h;
    float halfW, halfH, perPixel;
    std::vector<float> rgba; // linear, premultiplied
};
struct Corner {
    float x, y;
};
Corner toPixels(const Target& t, Vec3 p) {
    return {(p.x + t.halfW) / t.perPixel, (t.halfH - p.y) / t.perPixel};
}
bool owns(float w, float dx, float dy) {
    // a pixel centre exactly on an edge belongs to one of the two triangles sharing it
    return w > 0 || (w == 0 && (dy > 0 || (dy == 0 && dx < 0)));
}
// Samples a picture at (u, v) (0..1, v down), bilinear, edges clamped: linear rgb and alpha.
void sample(const mesh_model::Picture& p, float u, float v, float (&out)[4]) {
    const auto& lin = linearOfByte();
    const float x = std::clamp(u, 0.f, 1.f) * static_cast<float>(p.width) - .5f;
    const float y = std::clamp(v, 0.f, 1.f) * static_cast<float>(p.height) - .5f;
    const int x0 = std::clamp(static_cast<int>(std::floor(x)), 0, static_cast<int>(p.width) - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(y)), 0, static_cast<int>(p.height) - 1);
    const int x1 = std::min(x0 + 1, static_cast<int>(p.width) - 1), y1 = std::min(y0 + 1, static_cast<int>(p.height) - 1);
    const float fx = std::clamp(x - std::floor(x), 0.f, 1.f), fy = std::clamp(y - std::floor(y), 0.f, 1.f);
    const auto at = [&](int xx, int yy, int c) {
        const uint8_t b = p.rgba[(static_cast<size_t>(yy) * p.width + static_cast<size_t>(xx)) * 4 + static_cast<size_t>(c)];
        return c == 3 ? static_cast<float>(b) / 255.f : lin[b];
    };
    for (int c = 0; c < 4; ++c) {
        const float top = at(x0, y0, c) * (1 - fx) + at(x1, y0, c) * fx;
        const float bottom = at(x0, y1, c) * (1 - fx) + at(x1, y1, c) * fx;
        out[c] = top * (1 - fy) + bottom * fy;
    }
}
void fill(Target& t, const Vertex& a, const Vertex& b, const Vertex& c, const Portraits* portraits) {
    Corner p0 = toPixels(t, a.position), p1 = toPixels(t, b.position), p2 = toPixels(t, c.position);
    const Vertex* v0 = &a;
    const Vertex* v1 = &b;
    const Vertex* v2 = &c;
    float area = (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
    if (!std::isfinite(area) || std::abs(area) < 1e-6f)
        return;
    if (area < 0) {
        std::swap(p1, p2);
        std::swap(v1, v2);
        area = -area;
    }
    const mesh_model::Picture* image = nullptr;
    if (portraits) {
        const float layer = v0->shape.z;
        if (!(v0->shape.w > .5f) || !std::isfinite(layer) || layer < 0 ||
            static_cast<size_t>(layer) >= portraits->layers.size())
            return;
        image = &portraits->layers[static_cast<size_t>(layer)];
        if (!image->width || !image->height || image->rgba.size() < static_cast<size_t>(image->width) * image->height * 4)
            return;
    }
    const int xMin = std::max(0, static_cast<int>(std::floor(std::min({p0.x, p1.x, p2.x}))));
    const int xMax = std::min(t.w - 1, static_cast<int>(std::ceil(std::max({p0.x, p1.x, p2.x}))));
    const int yMin = std::max(0, static_cast<int>(std::floor(std::min({p0.y, p1.y, p2.y}))));
    const int yMax = std::min(t.h - 1, static_cast<int>(std::ceil(std::max({p0.y, p1.y, p2.y}))));
    if (xMin > xMax || yMin > yMax)
        return;
    // edge k is opposite corner k: w_k(p) = cross(edge, p - edge start), positive inside
    const Corner e0{p2.x - p1.x, p2.y - p1.y}, e1{p0.x - p2.x, p0.y - p2.y}, e2{p1.x - p0.x, p1.y - p0.y};
    const float inv = 1 / area;
    for (int y = yMin; y <= yMax; ++y) {
        const float py = static_cast<float>(y) + .5f;
        for (int x = xMin; x <= xMax; ++x) {
            const float px = static_cast<float>(x) + .5f;
            const float w0 = e0.x * (py - p1.y) - e0.y * (px - p1.x);
            const float w1 = e1.x * (py - p2.y) - e1.y * (px - p2.x);
            const float w2 = e2.x * (py - p0.y) - e2.y * (px - p0.x);
            if (!owns(w0, e0.x, e0.y) || !owns(w1, e1.x, e1.y) || !owns(w2, e2.x, e2.y))
                continue;
            const float b1 = w1 * inv, b2 = w2 * inv;
            // from corner 0 (exact where the corners agree: a flat colour stays flat)
            const auto mix = [&](float c0, float c1, float c2) { return c0 + (c1 - c0) * b1 + (c2 - c0) * b2; };
            float r = mix(v0->color.x, v1->color.x, v2->color.x);
            float g = mix(v0->color.y, v1->color.y, v2->color.y);
            float bl = mix(v0->color.z, v1->color.z, v2->color.z);
            float al = std::clamp(mix(v0->alpha, v1->alpha, v2->alpha), 0.f, 1.f);
            if (image) {
                float s[4];
                sample(*image, mix(v0->shape.x, v1->shape.x, v2->shape.x), mix(v0->shape.y, v1->shape.y, v2->shape.y), s);
                r *= s[0];
                g *= s[1];
                bl *= s[2];
                al *= s[3];
            }
            if (!(al > 0) || !std::isfinite(r + g + bl))
                continue;
            float* d = &t.rgba[(static_cast<size_t>(y) * static_cast<size_t>(t.w) + static_cast<size_t>(x)) * 4];
            const float keep = 1 - al;
            d[0] = std::max(r, 0.f) * al + d[0] * keep;
            d[1] = std::max(g, 0.f) * al + d[1] * keep;
            d[2] = std::max(bl, 0.f) * al + d[2] * keep;
            d[3] = al + d[3] * keep;
        }
    }
}
void fnv(uint64_t& h, const void* data, size_t n) {
    // FNV-1a over 64-bit words (then the tail bytes), with a final mix: fast enough to run every frame
    const auto* p = static_cast<const uint8_t*>(data);
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        uint64_t w;
        std::memcpy(&w, p + i, 8);
        h = (h ^ w) * 1099511628211ull;
        h ^= h >> 29;
    }
    for (; i < n; ++i)
        h = (h ^ p[i]) * 1099511628211ull;
}
std::string upper(std::string s) {
    for (char& c : s)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    return s;
}
} // namespace

bool loadPortraits(const uint8_t* glb, size_t size, mesh_model::Decoder decoder, Portraits& out, std::string& why) {
    out = Portraits{};
    if (!glb || !size) {
        why = "no Models\\Roster\\scene.glb";
        return false;
    }
    mesh_model::Mesh mesh;
    if (!mesh_model::parseGlb(glb, size, mesh, why))
        return false;
    for (size_t i = 0; i < mesh.images.size(); ++i)
        mesh.images[i].layer = static_cast<int32_t>(i);
    if (!roster_page::pictures(&mesh, out.at)) {
        why = "Models\\Roster\\scene.glb is not the Roster model";
        return false;
    }
    out.layers.resize(mesh.images.size());
    size_t decoded = 0;
    for (size_t i = 0; i < mesh.images.size(); ++i) {
        const auto& im = mesh.images[i];
        if (!im.bytes.empty() && mesh_model::decodeImage(im.bytes.data(), im.bytes.size(), decoder, out.layers[i]) &&
            out.layers[i].rgba.size() >= static_cast<size_t>(out.layers[i].width) * out.layers[i].height * 4)
            ++decoded;
        else
            out.layers[i] = {};
    }
    if (!decoded) {
        why = "none of the Roster model's " + std::to_string(mesh.images.size()) + " images decoded";
        out.layers.clear();
        return false;
    }
    // a picture on an image that did not decode keeps its tile's initials
    for (auto& p : out.at)
        if (p.layer >= 0 && (static_cast<size_t>(p.layer) >= out.layers.size() || out.layers[static_cast<size_t>(p.layer)].rgba.empty()))
            p.layer = -1;
    out.ok = true;
    why = std::to_string(decoded) + " of " + std::to_string(mesh.images.size()) + " images";
    return true;
}

void raster(const std::vector<Vertex>& translucent, const std::vector<Vertex>& textured, const Portraits* portraits,
            float halfW, float halfH, Canvas& out) {
    if (out.width <= 0 || out.height <= 0)
        return;
    Target t{out.width, out.height, halfW, halfH, 2 * halfW / static_cast<float>(out.width), {}};
    t.rgba.assign(static_cast<size_t>(t.w) * static_cast<size_t>(t.h) * 4, 0.f);
    for (size_t i = 0; i + 2 < translucent.size(); i += 3)
        fill(t, translucent[i], translucent[i + 1], translucent[i + 2], nullptr);
    if (portraits && portraits->ok)
        for (size_t i = 0; i + 2 < textured.size(); i += 3)
            fill(t, textured[i], textured[i + 1], textured[i + 2], portraits);
    out.pixels.assign(static_cast<size_t>(t.w) * static_cast<size_t>(t.h), 0);
    for (size_t i = 0; i < out.pixels.size(); ++i) {
        const float* s = &t.rgba[i * 4];
        const float a = std::clamp(s[3], 0.f, 1.f);
        if (a < 1.f / 512)
            continue;
        const uint32_t r = encode(s[0] / a), g = encode(s[1] / a), b = encode(s[2] / a);
        const uint32_t al = static_cast<uint32_t>(std::lround(a * 255));
        out.pixels[i] = al << 24 | b << 16 | g << 8 | r;
    }
}

uint64_t hashOf(const std::vector<Vertex>& translucent, const std::vector<Vertex>& textured) {
    uint64_t h = 14695981039346656037ull;
    const uint64_t n[2] = {translucent.size(), textured.size()};
    fnv(h, n, sizeof n);
    if (!translucent.empty())
        fnv(h, translucent.data(), translucent.size() * sizeof(Vertex));
    if (!textured.empty())
        fnv(h, textured.data(), textured.size() * sizeof(Vertex));
    return h;
}

void panel(const wrist_hud::View& view, std::vector<Vertex>& translucent, std::vector<Vertex>& textured) {
    translucent.clear();
    textured.clear();
    wrist_hud::View v = view;
    v.pointing = false;
    v.opacity = 1;
    if (v.rosterPictures)
        v.rosterImages = &textured;
    wrist_hud::appendPanelFlat(translucent, v, pixel);
}

void note(const std::string& title, const std::vector<std::string>& lines, std::vector<Vertex>& out) {
    out.clear();
    wrist_hud::appendNoticeFlat(out, title, lines, noteHalfWidth - .004f, notePixel, 0);
}

void paintDot(Canvas& c) {
    c = Canvas(dotSize, dotSize);
    c.clear(0);
    // SM2's pointer mark: a bright disc (its text colour) in a faint ring of its line colour.
    const float mid = (dotSize - 1) / 2.f;
    for (int y = 0; y < dotSize; ++y)
        for (int x = 0; x < dotSize; ++x) {
            const float d = std::hypot(static_cast<float>(x) - mid, static_cast<float>(y) - mid) / (dotSize / 2.f);
            float r = 0, g = 0, b = 0, a = 0;
            if (d < .52f) {
                r = .85f, g = .95f, b = 1, a = .95f * std::clamp((.52f - d) / .06f, 0.f, 1.f);
            } else if (d < .98f) {
                r = .1f, g = .5f, b = 1, a = .35f * std::clamp((.98f - d) / .08f, 0.f, 1.f);
            }
            c.pixels[static_cast<size_t>(y) * dotSize + static_cast<size_t>(x)] =
                static_cast<uint32_t>(std::lround(a * 255)) << 24 | static_cast<uint32_t>(encode(b)) << 16 |
                static_cast<uint32_t>(encode(g)) << 8 | encode(r);
        }
}

// --- What it shows --------------------------------------------------------------------------------------------------
const char* switchName(int key) {
    switch (key) {
    case swPowers: return "POWERS";
    case swBeams: return "BEAMS";
    case swFlight: return "FLIGHT";
    case swGrapple: return "GRAPPLE";
    case swHeroWebs: return "WEBS";
    default: return "";
    }
}
bool grapples(uint32_t hero) {
    return hero == hero_modes::captainAmerica || hero == hero_modes::daredevil;
}

holo_roster::Data rosterData(const Now& now) {
    using holo_roster::Sm1Line;
    using holo_roster::Sm1Switch;
    holo_roster::Data d;
    d.reputation = d.repLog = d.comingSoon = false;
    d.playing = now.config.heroes && now.config.hero < heroCount ? now.config.hero : 0;
    d.healthNa = true;
    // OUTFITS: HERO VR's roster (the SM2 build's catalog), the saved pick marked; picking saves it only.
    static const hero072::SuitCatalog catalog;
    d.suits = roster_page::snapshotOf(catalog, true, false);
    for (uint32_t h = 0; h < heroCount; ++h)
        for (uint32_t i = 0; i < d.suits.count[h]; ++i) {
            auto& r = d.suits.rows[h][i];
            if (r.status == roster_page::Status::missing)
                continue;
            r.status = i == now.config.outfit[h] ? roster_page::Status::inUse : roster_page::Status::unlocked;
        }
    d.suitsKnown = true;
    holo_roster::Sm1& s = d.sm1;
    s.on = true;
    s.outfitNote = "SAVED ONLY (NO CONFIRMED SUIT CALL). TO LOOK LIKE HIM: LAUNCHER'S INSTALL HERO SUITS, THEN THE GAME'S SUITS MENU";
    const Live& l = now.live;
    for (uint32_t h = 0; h < heroCount; ++h) {
        const Power& p = powerOf(h);
        const bool hasPower = p.source != Source::none;
        s.power[h] = hasPower ? p.name : "";
        auto& lines = s.lines[h];
        auto& sw = s.switches[h];
        if (h == hero_modes::spiderMan) {
            lines.push_back({"WEBS", "GRIPS: IILYN'S MOD, AS HE MADE IT"});
            lines.push_back({"FIGHT", "HIS PUNCHES AND KICKS"});
        } else {
            if (hasPower) {
                const char* aim = p.source == Source::eyes ? "LOOK AT AN ENEMY" : "POINT AT AN ENEMY";
                lines.push_back({p.name, std::string("STRETCH AN ARM OUT, PULL ITS TRIGGER: ") + aim});
            }
            if (flies(h)) {
                lines.push_back({"TAKE OFF", "BOTH ARMS STRAIGHT UP, OR HOLD A"});
                lines.push_back({"STEER", "POINT YOUR ARMS WHERE TO GO"});
                lines.push_back({"SPEED", "LEFT STICK FORWARD / BACK"});
                lines.push_back({"BRAKE / LAND", "HOLD B / HOLD A IN THE AIR"});
            }
            if (grapples(h)) {
                lines.push_back({"GRAPPLE", "HOLD A GRIP, PULL THAT TRIGGER AT A WALL"});
                lines.push_back({"LET GO / JUMP", "RELEASE THE GRIP / PRESS A"});
            }
            lines.push_back({"WEBS", l.heroWebs ? "GRIPS: IILYN'S SWINGING STAYS ON" : "OFF (WEBS SWITCH)"});
        }
        lines.push_back({"HOLOGRAM", "TOUCH LEFT WRIST, PULL RIGHT TRIGGER"});
        if (lines.size() > holo_roster::sm1LinesMost)
            lines.resize(holo_roster::sm1LinesMost);
        if (hasPower) {
            char buf[96];
            std::snprintf(buf, sizeof buf, "%s: %d DAMAGE, %d M, %.1f S", p.name,
                          static_cast<int>(std::lround(p.damage * static_cast<float>(l.damage) / 100.f)),
                          static_cast<int>(p.range), static_cast<double>(p.cooldown));
            s.facts[h] = buf;
            if (!now.powersWork && !now.why[0].empty())
                s.facts[h] += ". OFF: " + upper(now.why[0]);
            sw.push_back({"POWERS", l.powers, swPowers,
                          "Your hero's power on the trigger of an outstretched arm; off: the trigger is iilyn's again."});
            sw.push_back({"BEAMS", l.beams, swBeams, "Draws each shot's beam; off: the shots still land, unseen."});
        }
        if (flies(h)) {
            sw.push_back({"FLIGHT", now.flightSwitch && now.config.flight, swFlight,
                          now.config.flight ? "Flight for this hero: arms up or hold A to take off."
                                            : "Flight is off in the launcher (flight=0 in addon.ini)."});
            if (!now.flightWorks && !now.why[1].empty())
                s.facts[h] += (s.facts[h].empty() ? "" : ". ") + std::string("FLIGHT OFF: ") + upper(now.why[1]);
        }
        if (grapples(h)) {
            sw.push_back({"GRAPPLE", l.grapple, swGrapple, "The grapple on a held grip's trigger; off: the grips are iilyn's webs."});
            if (!now.grappleWorks && !now.why[2].empty())
                s.facts[h] += (s.facts[h].empty() ? "" : ". ") + std::string("GRAPPLE OFF: ") + upper(now.why[2]);
        }
        if (h != hero_modes::spiderMan)
            sw.push_back({"WEBS", l.heroWebs, swHeroWebs, "Keeps iilyn's web swinging on the grips for this hero."});
        if (sw.size() > holo_roster::sm1SwitchesMost)
            sw.resize(holo_roster::sm1SwitchesMost);
    }
    std::string st;
    if (d.playing != hero_modes::spiderMan) {
        if (powerOf(d.playing).source != Source::none)
            st = l.powers ? (now.powerState.empty() ? "POWERS READY" : "POWERS: " + now.powerState) : "POWERS OFF";
        if (now.flying)
            st += st.empty() ? "FLYING" : " / FLYING";
    }
    s.status = st;
    return d;
}

std::vector<std::string> playLines(uint32_t hero, const Now& now) {
    std::vector<std::string> out;
    if (hero == hero_modes::spiderMan || hero >= heroCount) {
        out.push_back("IILYN'S SPIDER-MAN, AS HE MADE HIM");
        return out;
    }
    const Power& p = powerOf(hero);
    if (p.source != Source::none)
        out.push_back(std::string(p.name) + (now.live.powers ? ": STRETCH AN ARM OUT, PULL ITS TRIGGER" : ": OFF (POWERS SWITCH)"));
    if (flies(hero))
        out.push_back(!(now.flightSwitch && now.config.flight) ? "FLIGHT IS OFF"
                      : now.flightLine.empty() ? "FLY: BOTH ARMS UP OR HOLD A" : now.flightLine);
    if (grapples(hero))
        out.push_back(now.live.grapple ? "GRAPPLE: HOLD A GRIP, PULL ITS TRIGGER" : "GRAPPLE IS OFF");
    out.push_back(now.suitLine.empty() ? "THE GAME'S MODEL STAYS (NO CONFIRMED SUIT CALL)" : now.suitLine);
    return out;
}

wrist_hud::Settings colorSettings(const Live& l) {
    wrist_hud::Settings s;
    s.colorStrength = static_cast<float>(l.colorStrength);
    s.exposure = static_cast<float>(l.colorExposure) / 10.f;
    s.contrast = static_cast<float>(l.colorContrast);
    s.saturation = static_cast<float>(l.colorSaturation);
    s.vibrance = static_cast<float>(l.colorVibrance);
    s.warmth = static_cast<float>(l.colorWarmth);
    s.tint = static_cast<float>(l.colorTint);
    s.vignette = static_cast<float>(l.colorVignette);
    return s;
}
void takeColor(const wrist_hud::Settings& s, Live& l) {
    // the SM2 build's own mapping (game_xr's COLOR slider)
    const auto whole = [](float f, int lo, int hi) {
        return std::clamp(static_cast<int>(std::lround(std::isfinite(f) ? f : 0.f)), lo, hi);
    };
    l.colorStrength = static_cast<uint32_t>(whole(s.colorStrength, 0, 100));
    l.colorExposure = whole(s.exposure * 10, -20, 20);
    l.colorContrast = static_cast<uint32_t>(whole(s.contrast, 50, 150));
    l.colorSaturation = static_cast<uint32_t>(whole(s.saturation, 0, 200));
    l.colorVibrance = static_cast<uint32_t>(whole(s.vibrance, 0, 100));
    l.colorWarmth = whole(s.warmth, -100, 100);
    l.colorTint = whole(s.tint, -100, 100);
    l.colorVignette = static_cast<uint32_t>(whole(s.vignette, 0, 100));
}
uint32_t colorBits(const Live& l) {
    return (l.colorFx ? 1u : 0u) | (std::min<uint32_t>(l.colorFilter, eye_color::filters - 1) & 0xffu) << 8;
}
uint32_t nextFilter(uint32_t filter) {
    return (filter + 1) % eye_color::filters;
}

// --- Painter --------------------------------------------------------------------------------------------------------
Painter::Painter() : thread_([this] { run(); }) {}
Painter::~Painter() {
    stop();
}
void Painter::stop() {
    {
        std::lock_guard<std::mutex> g(m_);
        quit_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable())
        thread_.join();
}
void Painter::setPortraits(std::shared_ptr<const Portraits> p) {
    std::lock_guard<std::mutex> g(m_);
    portraits_ = std::move(p);
}
void Painter::ask(uint64_t key, std::vector<Vertex> translucent, std::vector<Vertex> textured, float halfW, float halfH, int w,
                  int h) {
    {
        std::lock_guard<std::mutex> g(m_);
        if (asked_ == key && (pending_ || done_ || resultKey_ == key))
            return;
        asked_ = key;
        job_ = Job{key, std::move(translucent), std::move(textured), halfW, halfH, w, h};
        pending_ = true;
    }
    cv_.notify_all();
}
bool Painter::take(Canvas& out, uint64_t& key) {
    std::lock_guard<std::mutex> g(m_);
    if (!done_)
        return false;
    done_ = false;
    std::swap(out, result_);
    key = resultKey_;
    return true;
}
void Painter::run() {
    for (;;) {
        Job job;
        std::shared_ptr<const Portraits> portraits;
        {
            std::unique_lock<std::mutex> g(m_);
            cv_.wait(g, [this] { return quit_ || pending_; });
            if (quit_)
                return;
            job = std::move(job_);
            pending_ = false;
            portraits = portraits_;
        }
        Canvas c(job.w, job.h);
        raster(job.translucent, job.textured, portraits.get(), job.halfW, job.halfH, c);
        std::lock_guard<std::mutex> g(m_);
        result_ = std::move(c);
        resultKey_ = job.key;
        done_ = true;
    }
}
} // namespace spidy::sm1::holo
