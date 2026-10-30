// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
// 0.0.74 test 2b BATCAVE (batcave074.hpp).
#include "spidy/batcave074.hpp"
#include "spidy/overlay_text.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace spidy::batcave074 {

// FLAVOUR: original short files (the characters are DC's; every word here is ours).
const File criminalFiles[files] = {
    {"THE JOKER", "EXTREME", {"No plan anyone can predict, his own included.", "Laughs at pain - his and yours.", "Every escape ends with a body count."}},
    {"THE PENGUIN", "HIGH", {"Runs guns and favours through the docks.", "Buys the police he cannot scare.", "Never far from an umbrella or a getaway."}},
    {"THE RIDDLER", "HIGH", {"Leaves clues because he needs to be seen.", "Every trap has an answer - find it first.", "Hates being outsmarted more than caught."}},
    {"TWO-FACE", "HIGH", {"Former prosecutor. Lets a coin decide.", "Crews in pairs, jobs on the second.", "Both sides of him keep score."}},
    {"SCARECROW", "HIGH", {"Doctor of fear, maker of the toxin.", "Filters on before you go near him.", "What you see may not be there."}},
    {"POISON IVY", "MEDIUM", {"Plants answer to her; people too, briefly.", "Protects the green, not the city.", "Do not breathe the pollen."}},
    {"BANE", "EXTREME", {"Strength from the venom, mind from the dark.", "Studies you before he breaks you.", "Cut the tubes and he slows."}},
    {"HARLEY QUINN", "MEDIUM", {"Psychiatrist turned partner in chaos.", "Fast, acrobatic, unpredictable.", "Sometimes she wants out. Sometimes."}},
};

const char* const researchNotes[researchItems][2] = {
    {"GAME WAYPOINT LINK", "IN RESEARCH"},
    {"SUIT VAULT", "COMING SOON (THE VAULT POINT)"},
    {"EVIDENCE LAB", "COMING SOON (THE LAB BENCH)"},
    {"BATWING", "NOT STARTED"},
    {"REMOTE BATMOBILE SUMMON", "NOT STARTED"},
    {"GRAPPLE LAUNCHER", "NOT IN THIS BUILD"},
};

namespace {
constexpr float tau = 6.28318530718f;
bool fine(Vec3 v) { return finite(v); }

std::string clock(float s) {
    if (!std::isfinite(s) || s < 0)
        s = 0;
    const int t = static_cast<int>(s);
    char b[24];
    std::snprintf(b, sizeof b, "T+%d:%02d", t / 60, t % 60);
    return b;
}
std::string sel(bool on, const std::string& s) { return (on ? "> " : "  ") + s; }

struct Painter {
    std::vector<Vertex>& out;
    Vec3 c, r, u, n;
    float a = 1;
    float k = 1; // metres a design unit across the face (x, y and text; z stays metres)
    Vec3 P(float x, float y, float z = 0) const { return c + r * (x * k) + u * (y * k) + n * z; }
    void tri(Vec3 p, Vec3 q, Vec3 s, Vec3 col, float al) {
        out.push_back(Vertex{p, col, al * a});
        out.push_back(Vertex{q, col, al * a});
        out.push_back(Vertex{s, col, al * a});
    }
    void quad(float x0, float y0, float x1, float y1, float z, Vec3 col, float al = 1) {
        const Vec3 p00 = P(x0, y0, z), p10 = P(x1, y0, z), p11 = P(x1, y1, z), p01 = P(x0, y1, z);
        tri(p00, p10, p11, col, al);
        tri(p00, p11, p01, col, al);
    }
    void frame(float x0, float y0, float x1, float y1, float e, float z, Vec3 col, float al = 1) {
        quad(x0, y1 - e, x1, y1, z, col, al);
        quad(x0, y0, x1, y0 + e, z, col, al);
        quad(x0, y0 + e, x0 + e, y1 - e, z, col, al);
        quad(x1 - e, y0 + e, x1, y1 - e, z, col, al);
    }
    void disc(float x, float y, float rad, float z, Vec3 col, float al = 1, int k = 16) {
        for (int i = 0; i < k; ++i) {
            const float t0 = tau * i / k, t1 = tau * (i + 1) / k;
            tri(P(x, y, z), P(x + rad * std::cos(t0), y + rad * std::sin(t0), z), P(x + rad * std::cos(t1), y + rad * std::sin(t1), z), col, al);
        }
    }
    void ring(float x, float y, float rad, float w, float z, Vec3 col, float al = 1, int k = 40) {
        for (int i = 0; i < k; ++i) {
            const float t0 = tau * i / k, t1 = tau * (i + 1) / k, r0 = rad - w / 2, r1 = rad + w / 2;
            const Vec3 a0 = P(x + r0 * std::cos(t0), y + r0 * std::sin(t0), z), a1 = P(x + r0 * std::cos(t1), y + r0 * std::sin(t1), z),
                       b0 = P(x + r1 * std::cos(t0), y + r1 * std::sin(t0), z), b1 = P(x + r1 * std::cos(t1), y + r1 * std::sin(t1), z);
            tri(a0, b0, b1, col, al);
            tri(a0, b1, a1, col, al);
        }
    }
    void text(std::string_view s, float x, float y, float h, Vec3 col, float z = .003f, float al = 1, float weight = .13f) {
        TextStyle ts;
        ts.height = h * k;
        ts.weight = weight;
        ts.color = col;
        ts.soft = h * k * .09f;
        const size_t mark = out.size();
        appendText(out, s, P(x, y, z), r, u, ts);
        for (size_t i = mark; i < out.size(); ++i)
            out[i].alpha *= al * a;
    }
    void textCentre(std::string_view s, float x, float y, float h, Vec3 col, float z = .003f, float al = 1) {
        text(s, x - textWidth(s, h) / 2, y, h, col, z, al);
    }
};

// A billboard facing `viewer` about +y at world `at`.
Painter facing(std::vector<Vertex>& out, Vec3 at, Vec3 viewer) {
    Vec3 d{viewer.x - at.x, 0, viewer.z - at.z};
    d = fine(d) && length(d) > 1e-4f ? normalized(d) : Vec3{0, 0, 1};
    const Vec3 up{0, 1, 0};
    return Painter{out, at, cross(-d, up), up, d};
}

constexpr Vec3 gold{1.f, .70f, .16f}, cyan{.25f, .78f, 1.f}, grey{.45f, .47f, .50f}, ice{.80f, .93f, 1.f}, red{1.f, .16f, .10f},
               face{.004f, .014f, .030f}, trim{.06f, .32f, .55f}, amber{.95f, .62f, .08f};

void box(std::vector<Vertex>& out, const hideout074::Place& pl, Vec3 lo, Vec3 hi, Vec3 col) {
    const Vec3 c[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                       {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
    static const int f[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
    static const float shade[6] = {.8f, .8f, .65f, .65f, 1.f, .4f};
    for (int k = 0; k < 6; ++k) {
        Vec3 p[4];
        for (int i = 0; i < 4; ++i)
            p[i] = pl.world(c[f[k][i]]);
        const Vec3 cc = col * shade[k];
        out.push_back(Vertex{p[0], cc, 1});
        out.push_back(Vertex{p[1], cc, 1});
        out.push_back(Vertex{p[2], cc, 1});
        out.push_back(Vertex{p[0], cc, 1});
        out.push_back(Vertex{p[2], cc, 1});
        out.push_back(Vertex{p[3], cc, 1});
    }
}

bool allFine(const std::vector<Vertex>& v, size_t from) {
    for (size_t i = from; i < v.size(); ++i)
        if (!fine(v[i].position) || !fine(v[i].color) || !std::isfinite(v[i].alpha))
            return false;
    return true;
}
} // namespace

// ---- the computer ----

void Computer::open() {
    open_ = true;
    page_ = menu;
    cursor_ = 0;
    held_ = true; // the stick that was held when he sat does not move the cursor
    // the trigger / finger that sat him must be let go first (never a tap on the first frame)
    trigWas_[0] = trigWas_[1] = true;
    near_[0] = near_[1] = true;
}
void Computer::close() {
    open_ = false;
    page_ = menu;
    cursor_ = 0;
}

uint32_t Computer::rows(uint32_t live) const {
    switch (page_) {
    case menu: return pages - 1;
    case criminals: return files;
    case ongoing: return std::min(live, crime_gps074::mostCrimes);
    case music: return 2;
    case batmobile:
    case returnCity: return 1;
    default: return 0;
    }
}

Ask Computer::activate(uint32_t live) {
    const uint32_t n = rows(live);
    switch (page_) {
    case menu:
        page_ = std::min<uint32_t>(cursor_ + 1, pages - 1);
        cursor_ = 0;
        return Ask::none;
    case ongoing:
        if (!n)
            return Ask::none;
        trackIndex_ = std::min(cursor_, n - 1);
        return Ask::track;
    case batmobile: return Ask::nextCar;
    case music: return cursor_ == 0 ? Ask::musicToggle : Ask::musicNext;
    case returnCity: return Ask::returnToCity;
    default: return Ask::none;
    }
}

void buttonBox(Button b, float& x0, float& y0, float& x1, float& y1) {
    const float W = designWidth / 2, H = designHeight / 2;
    y0 = H - .098f;
    y1 = H - .022f;
    if (b == Button::stand)
        x0 = W - .25f, x1 = W - .03f;
    else
        x0 = W - .44f, x1 = W - .27f;
}

Button buttonAt(float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y))
        return Button::none;
    for (const Button b : {Button::back, Button::stand}) {
        float x0, y0, x1, y1;
        buttonBox(b, x0, y0, x1, y1);
        if (x >= x0 - .01f && x <= x1 + .01f && y >= y0 - .01f && y <= y1 + .01f)
            return b;
    }
    return Button::none;
}

int rowAt(float x, float y) {
    const float W = designWidth / 2;
    if (!std::isfinite(x) || !std::isfinite(y) || x < -W || x > W)
        return -1;
    for (uint32_t i = 1; i <= mostRows; ++i) {
        const float ry = rowTop - rowStep * static_cast<float>(i);
        // each row's band: its text with half the gap above and below (bands touch: no dead strips)
        const float lo = ry + rowText / 2 - rowStep / 2, hi = ry + rowText / 2 + rowStep / 2;
        if (y >= lo && y < hi)
            return static_cast<int>(i) - 1;
    }
    return -1;
}

Vec3 onPanel(Vec3 room) {
    const Vec3 d = room - screenCentre;
    return Vec3{d.x / screenScale, d.y / screenScale, d.z};
}

Ask Computer::tap(float x, float y, uint32_t live) {
    if (!open_)
        return Ask::none;
    const Button b = buttonAt(x, y);
    if (b == Button::stand) {
        close();
        return Ask::stand;
    }
    if (b == Button::back) {
        if (page_ == menu) {
            close();
            return Ask::stand;
        }
        cursor_ = page_ - 1;
        page_ = menu;
        return Ask::none;
    }
    const int r = rowAt(x, y);
    const uint32_t n = rows(live);
    if (r < 0 || static_cast<uint32_t>(r) >= n)
        return Ask::none;
    cursor_ = static_cast<uint32_t>(r);
    return activate(live);
}

Ask Computer::step(const In& in) {
    cursorOn_ = beamOn_ = false;
    hotRow_ = -1;
    hotButton_ = Button::none;
    if (!open_) {
        near_[0] = near_[1] = trigWas_[0] = trigWas_[1] = false;
        return Ask::none;
    }
    const uint32_t n = rows(in.liveCrimes);
    if (n && cursor_ >= n)
        cursor_ = n - 1;
    const float y = std::isfinite(in.stickY) ? in.stickY : 0.f;
    if (!held_ && std::abs(y) > .7f) {
        held_ = true;
        if (n)
            cursor_ = y > 0 ? (cursor_ + n - 1) % n : (cursor_ + 1) % n;
    } else if (held_ && std::abs(y) < .35f)
        held_ = false;
    // 0.0.74 test 2b BATCOMPUTER POINTER: the finger (a touch going in) first, then the ray + a fresh trigger.
    bool press = in.press;
    Ask asked = Ask::none;
    bool tapped = false;
    for (unsigned i = 0; i < 2; ++i) {
        const float tr = std::isfinite(in.trigger[i]) ? in.trigger[i] : 0.f;
        const bool pressed = trigWas_[i] ? tr > rayRelease : tr > rayPress;
        const bool fresh = pressed && !trigWas_[i];
        trigWas_[i] = pressed;
        if (!in.tracked[i] || !fine(in.hand[i])) {
            near_[i] = false;
            if (fresh)
                press = true;
            continue;
        }
        bool aimedHere = false;
        if (in.pointer) {
            Vec3 aim = fine(in.aim[i]) ? in.aim[i] : Vec3{};
            const float an = length(aim);
            const bool aimOk = std::isfinite(an) && an > .5f;
            if (aimOk)
                aim = aim * (1.f / an);
            const Vec3 tip = aimOk ? in.hand[i] + aim * fingerReach : in.hand[i];
            const Vec3 f = onPanel(tip);
            const float W = designWidth / 2, H = designHeight / 2;
            const bool inside = fine(f) && std::abs(f.x) <= W + .02f && std::abs(f.y) <= H + .02f;
            const bool near = inside && f.z <= pokeDepth && f.z >= -pokeThrough;
            const bool open = !std::isfinite(in.grip[i]) || in.grip[i] < .3f || tr < rayRelease; // a pointing hand
            if (near && open) {
                aimedHere = true;
                cursorOn_ = true;
                cursorAt_ = {std::clamp(f.x, -W, W), std::clamp(f.y, -H, H), 0};
                if (!near_[i] && !tapped) {
                    tapped = true;
                    asked = tap(f.x, f.y, in.liveCrimes);
                }
            }
            near_[i] = near;
            // the ray: from in front of the panel (+z side), toward it, not grazing, within reach
            const Vec3 o = onPanel(in.hand[i]);
            if (!near && aimOk && fine(o) && o.z > 0 && aim.z < -rayFacing) {
                const float t = o.z / -aim.z; // metres along the aim to the panel's plane
                const Vec3 hitRoom = in.hand[i] + aim * t;
                const Vec3 h = onPanel(hitRoom);
                if (t > 0 && t <= rayReach && fine(h) && std::abs(h.x) <= W + .03f && std::abs(h.y) <= H + .03f) {
                    aimedHere = true;
                    if (!cursorOn_) {
                        cursorOn_ = beamOn_ = true;
                        cursorAt_ = {std::clamp(h.x, -W, W), std::clamp(h.y, -H, H), 0};
                        beamFrom_ = in.hand[i];
                    }
                    if (fresh && !tapped) {
                        tapped = true;
                        asked = tap(h.x, h.y, in.liveCrimes);
                    }
                }
            }
        } else
            near_[i] = false;
        if (fresh && !aimedHere)
            press = true; // a trigger aimed elsewhere is A
    }
    if (cursorOn_) {
        hotRow_ = rowAt(cursorAt_.x, cursorAt_.y);
        if (hotRow_ >= 0 && static_cast<uint32_t>(hotRow_) >= rows(in.liveCrimes))
            hotRow_ = -1;
        hotButton_ = buttonAt(cursorAt_.x, cursorAt_.y);
    }
    if (tapped) {
        if (!open_)
            cursorOn_ = beamOn_ = false;
        return asked;
    }
    if (in.back) {
        if (page_ == menu) {
            close();
            return Ask::stand;
        }
        cursor_ = page_ - 1;
        page_ = menu;
        return Ask::none;
    }
    if (!press)
        return Ask::none;
    return activate(in.liveCrimes);
}

// ---- the pages ----

std::vector<std::string> pageLines(uint32_t page, uint32_t cursor, const Feed& f) {
    std::vector<std::string> L;
    if (page >= pages)
        page = menu;
    L.emplace_back(page == menu ? std::string("BATCOMPUTER") : std::string(pageNames[page])); // test 2b polish: the page name only (no overlap with BACK)
    const auto* t = f.crimes;
    char b[160];
    const auto where = [&](Vec3 at, float& m, float& deg) { return f.cityOk && crime_gps074::bearingTo(f.city, at, deg, m); };
    switch (page) {
    case menu:
        for (uint32_t i = 1; i < pages; ++i)
            L.push_back(sel(cursor + 1 == i, pageNames[i]));
        break;
    case criminals: {
        for (uint32_t i = 0; i < files; ++i)
            L.push_back(sel(cursor == i, criminalFiles[i].name));
        const File& c = criminalFiles[std::min(cursor, files - 1)];
        L.push_back(std::string("THREAT: ") + c.threat);
        for (const char* l : c.lines)
            L.emplace_back(l);
        break;
    }
    case ongoing: {
        uint32_t row = 0;
        if (t)
            for (const auto& c : t->live()) {
                if (!c.id)
                    continue;
                float m = 0, deg = 0;
                if (where(c.at, m, deg))
                    std::snprintf(b, sizeof b, "CRIME %u  %u SUSPECTS  %.0f M %s%s", c.id, c.thugs, m, crime_gps074::compass(deg),
                                  t->tracked() == c.id ? "  TRACKED" : "");
                else
                    std::snprintf(b, sizeof b, "CRIME %u  %u SUSPECTS%s", c.id, c.thugs, t->tracked() == c.id ? "  TRACKED" : "");
                L.push_back(sel(cursor == row++, b));
            }
        if (!row)
            L.emplace_back("NO ONGOING CASES SEEN - DRIVE WITH CRIME GPS ON");
        L.emplace_back("");
        L.emplace_back("A / TRIGGER: TRACK IT (THE CAR'S MAP AND ARROW)");
        break;
    }
    case pastCases: {
        const uint32_t n = t ? t->pastCount() : 0;
        for (uint32_t k = 0; k < n; ++k) {
            const auto& p = t->past(k);
            std::snprintf(b, sizeof b, "CRIME %u  %s  %u SUSPECTS  %.0f S  %s", p.id,
                          p.outcome == crime_gps074::Outcome::cleared ? "CLEARED" : "LOST", p.most, p.lasted, clock(p.ended).c_str());
            L.emplace_back(b);
        }
        if (!n)
            L.emplace_back("NO PAST CASES THIS SESSION");
        L.emplace_back("");
        L.emplace_back("CLEARED: YOU WERE NEAR WHEN IT ENDED; LOST: NOT");
        break;
    }
    case scanner: {
        const uint32_t n = t ? t->logCount() : 0;
        static const char* kinds[] = {"", "SPOTTED", "TRACKING", "CLEARED", "LOST"};
        for (uint32_t k = 0; k < n && k < 9; ++k) {
            const auto& l = t->log(k);
            std::snprintf(b, sizeof b, "%s  %s CRIME %u  %u SUSPECTS  %.0f M %s", clock(l.at).c_str(), kinds[static_cast<int>(l.kind) % 5],
                          l.id, l.thugs, l.metres, crime_gps074::compass(l.bearing));
            L.emplace_back(b);
        }
        if (!n)
            L.emplace_back("NO TRAFFIC YET - DRIVE WITH CRIME GPS ON");
        L.emplace_back("");
        L.emplace_back("DISPATCH: ALL UNITS, GIVE THE BLACK CAR ROOM");
        break;
    }
    case research:
        L.emplace_back("PLANNED - NOTHING ON THIS PAGE WORKS YET");
        for (uint32_t i = 0; i < researchItems; ++i)
            L.push_back(std::string(researchNotes[i][0]) + ": " + researchNotes[i][1]);
        break;
    case map:
        L.emplace_back("RED: CRIME  GOLD: TRACKED");
        L.emplace_back("CYAN: THE CAR");
        L.emplace_back("RINGS: 200 M, 400 M");
        L.emplace_back("NORTH UP");
        L.emplace_back("STYLIZED - NO GAME MAP");
        if (!f.cityOk)
            L.emplace_back("NO CAR POSITION");
        break;
    case batmobile:
        L.push_back(sel(true, std::string("CAR: ") + (f.car && *f.car ? f.car : "?")));
        L.emplace_back("A / TRIGGER: THE NEXT ONE (USED ON THE NEXT SUMMON)");
        break;
    case music:
        L.push_back(sel(cursor == 0, f.musicPlaying ? "PAUSE" : "PLAY"));
        L.push_back(sel(cursor == 1, "NEXT SONG"));
        L.emplace_back(f.musicOn ? (f.song.empty() ? std::string("NOW: -") : "NOW: " + f.song.substr(0, 48))
                                 : std::string("NO MUSIC FOUND (THE CAR MUSIC FOLDER)"));
        break;
    case returnCity:
        L.push_back(sel(true, f.batman ? "RETURN TO CITY - BACK IN THE BATMOBILE" : "RETURN TO CITY - BACK IN THE VAN"));
        break;
    default: break;
    }
    return L;
}

std::vector<Vec3> mapDots(const Feed& f, float range) {
    std::vector<Vec3> d;
    if (!f.cityOk || !fine(f.city) || !(range > 1))
        return d;
    d.push_back(Vec3{0, 2, 0});
    if (!f.crimes)
        return d;
    for (const auto& c : f.crimes->live()) {
        if (!c.id || !fine(c.at))
            continue;
        float x = (c.at.x - f.city.x) / range, y = -(c.at.z - f.city.z) / range;
        const float l = std::hypot(x, y);
        if (l > 1)
            x /= l, y /= l; // pinned to the edge
        d.push_back(Vec3{x, f.crimes->tracked() == c.id ? 1.f : 0.f, y});
    }
    return d;
}

bool WristButton::step(bool on, Vec3 at, Vec3 tip, bool tracked) {
    if (!on || !tracked || !fine(at) || !fine(tip)) {
        armed_ = lit_ = false;
        return false;
    }
    const float d = length(tip - at);
    lit_ = d < releaseMetres;
    if (d > releaseMetres)
        armed_ = true;
    if (armed_ && d < pokeMetres) {
        armed_ = false;
        return true;
    }
    return false;
}

// ---- the draw ----

bool append(std::vector<Vertex>& solid, std::vector<Vertex>& glow, const View& v, Vec3 viewer) {
    if (!v.on)
        return false;
    const size_t s0 = solid.size(), g0 = glow.size();
    try {
        const auto& pl = v.place;
        if (!fine(pl.at) || !std::isfinite(pl.yaw) || !fine(viewer))
            return false;
        const float pulse = .5f + .5f * std::sin(v.seconds * tau * .7f);
        // the markers
        for (uint32_t k = 0; k < std::min(v.marks, View::mostMarks); ++k) {
            const Vec3 at = v.markAt[k];
            const int kind = v.markKind[k];
            const bool lit = v.markLit == static_cast<int>(k);
            const Vec3 col = kind == 0 ? gold : kind == 1 ? cyan : grey;
            const float al = (kind == 2 ? .35f : .55f + .3f * pulse) * (lit ? 1.3f : 1.f);
            // floor rings (room x-z plane)
            Painter fl{glow, pl.world(Vec3{at.x, at.y + .03f, at.z}), pl.worldDir({1, 0, 0}), pl.worldDir({0, 0, -1}), {0, 1, 0}};
            fl.ring(0, 0, .55f + .08f * pulse, .05f, 0, col, std::min(1.f, al));
            if (kind != 2)
                fl.ring(0, 0, .85f, .025f, 0, col, std::min(1.f, al * .6f));
            // the beam and the label, facing the eye
            const Vec3 top = pl.world(Vec3{at.x, at.y + (kind == 2 ? 1.6f : 2.3f), at.z});
            Painter bb = facing(glow, pl.world(at), viewer);
            if (kind != 2) {
                bb.quad(-.05f, 0, .05f, 2.2f, 0, col, .18f + .12f * pulse);
                bb.quad(-.015f, 0, .015f, 2.2f, .001f, col, .45f);
            }
            Painter lb = facing(glow, top, viewer);
            const char* label = kind == 0 ? "RETURN TO CITY" : kind == 1 ? "BATCOMPUTER" : "COMING SOON";
            const float h = kind == 2 ? .07f : .13f;
            lb.quad(-textWidth(label, h) / 2 - .05f, -.04f, textWidth(label, h) / 2 + .05f, h + .05f, -.002f, face, .55f);
            lb.textCentre(label, 0, 0, h, kind == 2 ? grey : ice, .002f, 1);
            if (kind == 0)
                lb.textCentre(lit ? "A / TRIGGER" : "WALK HERE", 0, -.11f, .055f, col, .002f, .9f);
        }
        // the arrival hint
        if (v.hintFade > 0.01f && fine(v.hintAt) && fine(v.hintFacing)) {
            Vec3 d = pl.worldDir(v.hintFacing);
            d = Vec3{d.x, 0, d.z};
            d = length(d) > 1e-4f ? normalized(d) : Vec3{0, 0, 1};
            Painter h{glow, pl.world(v.hintAt), cross(-d, Vec3{0, 1, 0}), {0, 1, 0}, d, std::min(1.f, v.hintFade)};
            h.quad(-.62f, -.25f, .62f, .25f, 0, face, .75f);
            h.frame(-.62f, -.25f, .62f, .25f, .008f, .001f, trim, .9f);
            h.textCentre(v.batcave ? "THE BATCAVE" : "THE SAFEHOUSE", 0, .13f, .07f, ice);
            h.textCentre("WALK: LEFT STICK   TURN: RIGHT STICK", 0, .03f, .04f, cyan);
            h.textCentre(v.batcave ? "RETURN TO CITY: THE GLOWING BATMOBILE OR BLAST DOOR," : "RETURN TO CITY: THE GLOWING VAN,", 0, -.06f, .036f, gold);
            h.textCentre("THE WRIST BUTTON, OR HOLD B", 0, -.13f, .036f, gold);
            if (v.batcave)
                h.textCentre("BATCOMPUTER: THE BLUE RING ON THE PLATFORM", 0, -.21f, .03f, cyan);
        }
        // the wrist button
        if (v.wristOn && fine(v.wristAt)) {
            Painter w = facing(glow, v.wristAt, viewer);
            const float al = v.wristLit ? 1.f : .8f;
            w.quad(-.075f, -.022f, .075f, .03f, 0, face, .8f * al);
            w.frame(-.075f, -.022f, .075f, .03f, .003f, .0005f, v.wristLit ? ice : gold, al);
            w.textCentre("RETURN TO CITY", 0, -.006f, .016f, v.wristLit ? ice : gold, .001f, al);
            w.textCentre("POKE", 0, -.036f, .01f, grey, .001f, .8f);
        }
        if (v.batcave) {
            // the chair (opaque): a seat, a back toward +z (he faces -z), a post
            const Vec3 f = chairFeet;
            const Vec3 dark{.05f, .055f, .06f};
            box(solid, pl, {f.x - .04f, f.y, f.z - .04f}, {f.x + .04f, f.y + .40f, f.z + .04f}, {.10f, .10f, .11f});
            box(solid, pl, {f.x - .30f, f.y, f.z - .03f}, {f.x + .30f, f.y + .05f, f.z + .03f}, {.10f, .10f, .11f});
            box(solid, pl, {f.x - .03f, f.y, f.z - .30f}, {f.x + .03f, f.y + .05f, f.z + .30f}, {.10f, .10f, .11f});
            box(solid, pl, {f.x - .27f, f.y + .40f, f.z - .26f}, {f.x + .27f, f.y + .49f, f.z + .24f}, dark);
            box(solid, pl, {f.x - .26f, f.y + .49f, f.z + .20f}, {f.x + .26f, f.y + 1.12f, f.z + .27f}, dark);
            // the screen: a holo panel within reach of the seat (design units, screenScale metres a unit)
            if (v.computer) {
                Painter s{glow, pl.world(screenCentre), pl.worldDir({1, 0, 0}), pl.worldDir({0, 1, 0}), pl.worldDir({0, 0, 1})};
                s.k = screenScale;
                const float W = designWidth / 2, H = designHeight / 2;
                s.quad(-W, -H, W, H, 0, face, .88f);
                s.frame(-W, -H, W, H, .01f, .001f, trim, 1);
                const size_t n = v.lines.size();
                // test 2b polish: the title shrinks to end before BACK; the rows before the panel's edge (MAP: the map)
                const auto fit = [](const std::string& t, float h, float room) {
                    const float w = textWidth(t, h);
                    return w > room && w > 1e-6f ? h * room / w : h;
                };
                if (n) {
                    float bx0, by0, bx1, by1;
                    buttonBox(Button::back, bx0, by0, bx1, by1);
                    s.text(v.lines[0], -W + .05f, H - .09f, fit(v.lines[0], .052f, bx0 - .03f - (-W + .05f)), ice, .002f);
                }
                s.quad(-W + .04f, H - .115f, W - .04f, H - .11f, .001f, trim, .9f);
                // BACK / STAND UP
                for (const Button bt : {Button::back, Button::stand}) {
                    float x0, y0, x1, y1;
                    buttonBox(bt, x0, y0, x1, y1);
                    const bool hot = v.hotButton == static_cast<int>(bt);
                    s.quad(x0, y0, x1, y1, .001f, hot ? trim : face, hot ? .95f : .7f);
                    s.frame(x0, y0, x1, y1, .006f, .0015f, hot ? ice : amber, 1);
                    s.textCentre(bt == Button::back ? (v.page == menu ? "CLOSE" : "BACK") : "STAND UP", (x0 + x1) / 2, y0 + .022f, .034f,
                                 hot ? ice : amber, .002f);
                }
                const bool isMap = v.page == map;
                for (size_t i = 1; i < n && i <= mostRows; ++i) {
                    const std::string& l = v.lines[i];
                    const float y = rowTop - rowStep * static_cast<float>(i);
                    const bool picked = l.size() > 1 && l[0] == '>';
                    const bool hot = v.hotRow == static_cast<int>(i) - 1;
                    if (picked || hot)
                        s.quad(-W + .04f, y - .012f, (isMap ? 0.f : W - .04f), y + rowText + .012f, .001f, hot ? ice : trim,
                               hot ? (v.touching ? .5f : .3f) : .55f);
                    s.text(l, -W + .06f, y, fit(l, rowText, (isMap ? .03f : W - .05f) - (-W + .06f)), picked || hot ? ice : cyan, .002f);
                }
                if (isMap) {
                    const float mx = .42f, my = -.04f, R = .36f;
                    s.ring(mx, my, R, .006f, .002f, trim, .9f);
                    s.ring(mx, my, R / 2, .004f, .002f, trim, .7f);
                    s.quad(mx - .002f, my - R, mx + .002f, my + R, .002f, trim, .4f);
                    s.quad(mx - R, my - .002f, mx + R, my + .002f, .002f, trim, .4f);
                    s.textCentre("N", mx, my + R + .015f, .035f, ice);
                    for (const Vec3& d : v.mapDots) {
                        if (!fine(d))
                            continue;
                        const int kind = static_cast<int>(d.y);
                        s.disc(mx + d.x * R, my + d.z * R, kind == 2 ? .014f : .018f, .003f, kind == 2 ? cyan : kind == 1 ? gold : red, 1);
                        if (kind == 1) {
                            s.ring(mx + d.x * R, my + d.z * R, .03f + .01f * pulse, .005f, .003f, gold, .9f);
                            // the route (CANDIDATE: a straight line car -> crime, not the game's roads)
                            const float dx = d.x * R, dy = d.z * R, L = std::hypot(dx, dy);
                            if (L > .02f) {
                                const float px = -dy / L * .004f, py = dx / L * .004f;
                                const Vec3 p0 = s.P(mx + px, my + py, .0025f), p1 = s.P(mx - px, my - py, .0025f),
                                           p2 = s.P(mx + dx - px, my + dy - py, .0025f), p3 = s.P(mx + dx + px, my + dy + py, .0025f);
                                s.tri(p0, p1, p2, gold, .8f);
                                s.tri(p0, p2, p3, gold, .8f);
                            }
                        }
                    }
                }
                s.text("TAP OR AIM + TRIGGER   STICK: PICK   A: OPEN   B: BACK", -W + .05f, -H + .03f, .028f, amber, .002f, .9f);
                // the pointer: the cursor and (aimed from afar) the beam
                if (v.cursorOn && fine(v.cursorAt)) {
                    const float cx = std::clamp(v.cursorAt.x, -W, W), cy = std::clamp(v.cursorAt.y, -H, H);
                    s.disc(cx, cy, .012f, .004f, ice, 1);
                    s.ring(cx, cy, .026f, .005f, .004f, v.touching ? gold : ice, .9f);
                    if (v.beamOn && fine(v.beamFrom)) {
                        const Vec3 to = s.P(cx, cy, .004f), from = pl.world(v.beamFrom);
                        Vec3 d = to - from;
                        const float L = length(d);
                        if (std::isfinite(L) && L > .02f && L < 4.f) {
                            d = d * (1.f / L);
                            Vec3 side = cross(d, viewer - from);
                            const float sl = length(side);
                            side = std::isfinite(sl) && sl > 1e-5f ? side * (.0025f / sl) : Vec3{.0025f, 0, 0};
                            const Vec3 q[4] = {from + side, from - side, to - side, to + side};
                            Painter bm{glow, from, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
                            bm.tri(q[0], q[1], q[2], cyan, .7f);
                            bm.tri(q[0], q[2], q[3], cyan, .7f);
                        }
                    }
                }
            }
        }
        if (!allFine(solid, s0) || !allFine(glow, g0)) {
            solid.resize(s0);
            glow.resize(g0);
            return false;
        }
        return true;
    } catch (...) {
        solid.resize(s0);
        glow.resize(g0);
        return false;
    }
}

} // namespace spidy::batcave074
