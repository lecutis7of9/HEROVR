#include "spidy/roster_page.hpp"
#include "spidy/hero_controls.hpp"
#include "spidy/overlay_text.hpp"
#include "spidy/test_notice.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace spidy::roster_page {
namespace {
bool okPose(const Pose& p) {
    const Quat& q = p.orientation;
    return finite(p.position) && std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w) &&
           std::abs(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w - 1.f) < .05f;
}
bool hitPlane(Pose panel, Vec3 from, Vec3 way, Vec3& local) {
    const Vec3 n = panel.orientation.rotate({0, 0, 1});
    const float d = dot(way, n);
    if (!(std::abs(d) > 1e-4f))
        return false;
    const float t = dot(panel.position - from, n) / d;
    if (!(t > 0 && t < 20.f))
        return false;
    local = panel.orientation.conjugate().rotate(from + way * t - panel.position);
    return finite(local);
}
const Button* find(const Button* b, uint32_t n, Vec3 local, float slack) {
    for (uint32_t i = 0; i < n; ++i)
        if (local.x >= b[i].x0 - slack && local.x <= b[i].x1 + slack && local.y >= b[i].y0 - slack && local.y <= b[i].y1 + slack)
            return &b[i];
    return nullptr;
}
std::mutex gLock;
Snapshot gSnap{};
uint32_t gVersion{};

// ---- the layout (panel space; the buttons and the drawing share it) ----
constexpr float W = width / 2, H = height / 2;
constexpr float closeX0 = .42f, closeX1 = .58f, closeY0 = .335f, closeY1 = .41f;
// grid
constexpr float gridX = -.58f, gridTop = .31f, cellW = .29f, cellH = .238f, tilePic = .172f;
// hero page
constexpr float portX0 = -.58f, portX1 = -.25f, portY1 = .40f, portY0 = .07f;
constexpr float playY0 = -.335f, playY1 = -.265f, backY0 = -.43f, backY1 = -.36f;
constexpr float rowsX0 = -.21f, rowsX1 = .58f, rowsTop = .325f, rowsBottom = -.245f;
// 0.0.73 test 2 (fix 23): the SUITS and CONTROLS tabs, in the title row left of CLOSE.
constexpr float tabY0 = .338f, tabY1 = .405f, suitsX0 = -.21f, suitsX1 = .03f, controlsX0 = .05f, controlsX1 = .37f;
float rowHeight(uint32_t n) {
    return std::min(.095f, (rowsTop - rowsBottom) / static_cast<float>(std::max(1u, n)));
}
void cellOf(uint32_t k, float& x0, float& top) {
    x0 = gridX + static_cast<float>(k % 4) * cellW;
    top = gridTop - static_cast<float>(k / 4) * cellH;
}
const char* shortName(uint32_t h) {
    return hero072::bio(h).name;
}
} // namespace

int pictureOf(const char* folder) {
    if (!folder || !folder[0])
        return -1;
    for (uint32_t i = 0; i < pictureCount; ++i)
        if (std::strcmp(pictureFolders[i], folder) == 0)
            return static_cast<int>(i);
    return -1;
}

Snapshot snapshotOf(const hero072::SuitCatalog& c, bool all, bool ownSuit) {
    Snapshot s;
    s.ownSuit = ownSuit;
    s.all = all;
    for (uint32_t h = 0; h < heroes; ++h) {
        const uint32_t n = std::min(suitsMost, c.suits(h));
        const uint32_t worn = c.inUse(h, all, ownSuit);
        s.count[h] = n;
        for (uint32_t i = 0; i < n; ++i) {
            const hero072::Suit* x = c.suit(h, i);
            if (!x)
                continue;
            Row& r = s.rows[h][i];
            std::memcpy(r.name, x->name, sizeof r.name);
            std::memcpy(r.artist, x->artist, sizeof r.artist);
            std::memcpy(r.licence, x->licence, sizeof r.licence);
            std::memcpy(r.folder, x->folder, sizeof r.folder);
            r.final = x->final;
            r.rep = x->rep;
            if (i == 0)
                r.status = worn == 0 ? Status::inUse : Status::unlocked;
            else if (x->soon)
                r.status = Status::missing;
            else if (!c.isUnlocked(h, i, all))
                r.status = x->final ? Status::lockedFinal : Status::lockedRep;
            else
                r.status = worn == i ? Status::inUse : Status::unlocked;
        }
    }
    return s;
}
bool same(const Snapshot& a, const Snapshot& b) {
    // 0.0.73 fix 1 (ROSTER FLICKER): field by field. memcmp read the structs' padding too, which the
    // game thread's fresh snapshot leaves unset: it "differed" every frame, so the version grew every
    // frame and the render thread rebuilt the whole page (plate, text, pictures) every frame.
    if (a.ownSuit != b.ownSuit || a.all != b.all)
        return false;
    for (uint32_t h = 0; h < heroes; ++h) {
        if (a.count[h] != b.count[h])
            return false;
        for (uint32_t i = 0; i < suitsMost; ++i) {
            const Row &x = a.rows[h][i], &y = b.rows[h][i];
            if (x.status != y.status || x.final != y.final || x.rep != y.rep || std::memcmp(x.name, y.name, sizeof x.name) ||
                std::memcmp(x.artist, y.artist, sizeof x.artist) || std::memcmp(x.licence, y.licence, sizeof x.licence) ||
                std::memcmp(x.folder, y.folder, sizeof x.folder))
                return false;
        }
    }
    return true;
}
void publish(const Snapshot& s) {
    std::lock_guard<std::mutex> g(gLock);
    if (gVersion && same(s, gSnap))
        return;
    gSnap = s;
    ++gVersion;
}
uint32_t version() {
    std::lock_guard<std::mutex> g(gLock);
    return gVersion;
}
uint32_t latest(Snapshot& out) {
    std::lock_guard<std::mutex> g(gLock);
    out = gSnap;
    return gVersion;
}

uint32_t buttons(Screen screen, uint32_t hero, const Snapshot* s, Button (&out)[buttonsMost]) {
    uint32_t n = 0;
    if (screen == Screen::closed)
        return 0;
    out[n++] = Button{closeButton, closeX0, closeY0, closeX1, closeY1, true};
    if (screen == Screen::grid) {
        for (uint32_t k = 0; k < heroes; ++k) {
            float x0, top;
            cellOf(k, x0, top);
            out[n++] = Button{tile + static_cast<int>(k), x0 + .01f, top - cellH + .01f, x0 + cellW - .01f, top, true};
        }
        return n;
    }
    hero = std::min(hero, heroes - 1);
    out[n++] = Button{playButton, portX0, playY0, portX1, playY1, true};
    out[n++] = Button{backButton, portX0, backY0, portX1, backY1, true};
    out[n++] = Button{suitsTab, suitsX0, tabY0, suitsX1, tabY1, screen == Screen::controls};
    out[n++] = Button{controlsTab, controlsX0, tabY0, controlsX1, tabY1, screen == Screen::hero};
    if (s && screen == Screen::hero) {
        const uint32_t rows = std::min(suitsMost, s->count[hero]);
        const float rh = rowHeight(rows);
        for (uint32_t i = 0; i < rows && n < buttonsMost; ++i) {
            const Status st = s->rows[hero][i].status;
            const float top = rowsTop - static_cast<float>(i) * rh;
            out[n++] = Button{row + static_cast<int>(i), rowsX0, top - rh + .006f, rowsX1, top,
                              st == Status::unlocked || st == Status::inUse};
        }
    }
    return n;
}

std::string helpFor(Screen screen, uint32_t hero, int id, const Snapshot* s) {
    hero = std::min(hero, heroes - 1);
    const std::string who = shortName(hero);
    if (id == closeButton)
        return "Closes the roster (B or Y does too) and gives your hands back to the game. No restart needed.";
    if (screen == Screen::grid) {
        if (id >= tile && id < tile + static_cast<int>(heroes))
            return std::string("Opens ") + shortName(static_cast<uint32_t>(id - tile)) +
                   "'s page: his story, PLAY AS and his suits. No restart needed.";
        return "Aim a hand and pull its trigger, or poke a tile, to open a hero; B or Y closes the roster.";
    }
    if (id == playButton)
        return "Switches you to " + who + " right now, the same as his hero toggle on the wrist. No restart needed.";
    if (id == backButton)
        return "Back to every hero's picture (B or Y does the same). No restart needed.";
    if (id == suitsTab)
        return "Shows " + who + "'s suits to pick from. No restart needed.";
    if (id == controlsTab)
        return "Shows how to play " + who + ": every move and the button or gesture for it. No restart needed.";
    if (screen == Screen::controls)
        return "How to play " + who + ". B or Y goes back to his suits. No restart needed.";
    if (id >= row && s && static_cast<uint32_t>(id - row) < s->count[hero]) {
        const uint32_t i = static_cast<uint32_t>(id - row);
        const Row& r = s->rows[hero][i];
        if (i == 0)
            return std::string(hero == hero_modes::captainAmerica
                                   ? "Keeps the suit the game or your own suit mod gives him (the CapSuit swap is done on your PC). "
                                   : "Keeps the suit the game or your own suit mod gives him, never one of ours. ") +
                   "No restart needed.";
        switch (r.status) {
        case Status::missing:
            return std::string("This suit's model is not downloaded yet, so it cannot be worn and no stand-in is shown") +
                   (r.final ? " (his FINAL: it unlocks at max reputation)" : "") + ". No restart needed.";
        case Status::lockedFinal:
            return "His FINAL suit: it unlocks at his max reputation (or with UNLOCK ALL SUITS). No restart needed.";
        case Status::lockedRep: {
            char t[128];
            std::snprintf(t, sizeof t, "Unlocks at %d reputation as %s. No restart needed.", static_cast<int>(r.rep), who.c_str());
            return t;
        }
        case Status::inUse:
            return "This is his suit now (kept with your progress). No restart needed.";
        case Status::unlocked:
            return "Makes this his suit (kept with your progress). No restart needed.";
        }
    }
    if (s && s->ownSuit)
        return "OWN SUIT is on in the wrist's settings, so every hero keeps his own mod or game suit.";
    return "Your pick is kept with your progress; alt suits are not drawn on your body in this test build yet.";
}

bool Page::open(Pose head, double seconds) {
    if (!okPose(head))
        return false;
    Vec3 ahead = head.orientation.rotate({0, 0, -1});
    ahead.y = 0;
    ahead = length(ahead) > .3f ? normalized(ahead) : Vec3{0, 0, -1};
    panel_ = {head.position + ahead * distance + Vec3{0, -drop, 0}, Quat::yaw(std::atan2(-ahead.x, -ahead.z))};
    if (!okPose(panel_))
        return false;
    screen_ = Screen::grid;
    hover_ = none;
    openedAt_ = last_ = seconds;
    opacity_ = 0;
    for (unsigned i = 0; i < 2; ++i) {
        released_[i] = aReleased_[i] = bReleased_[i] = false; // what is held as it opens is not a press
        wasNear_[i] = true;
        laserOn_[i] = laserWas_[i] = false;
    }
    hoverSeen_ = seconds;
    return true;
}
void Page::close() {
    if (screen_ != Screen::closed)
        latch_ = true;
    screen_ = Screen::closed;
    hover_ = none;
    opacity_ = 0;
    laserOn_[0] = laserOn_[1] = laserWas_[0] = laserWas_[1] = false;
}
bool Page::laser(unsigned hand, Vec3& from, Vec3& to) const {
    if (hand > 1 || !laserOn_[hand])
        return false;
    from = laserFrom_[hand], to = laserTo_[hand];
    return true;
}

Actions Page::update(const Input& in) {
    Actions act;
    if (screen_ == Screen::closed) {
        bool held = false;
        for (const auto& h : in.hands)
            held = held || h.a || h.b || h.trigger > triggerOff;
        latch_ = latch_ && held;
        return act;
    }
    const double dt = std::clamp(in.seconds - last_, 0.0, .1);
    last_ = in.seconds;
    opacity_ = std::min(1.f, opacity_ + static_cast<float>(dt / fadeSeconds));
    const bool armed = in.seconds - openedAt_ >= armSeconds;
    if (const uint32_t v = version(); v != snapVersion_)
        snapVersion_ = latest(snap_);
    const Snapshot* s = snapVersion_ ? &snap_ : nullptr;
    Button bs[buttonsMost];
    const uint32_t n = buttons(screen_, hero_, s, bs);
    const int hoverWas = hover_;
    const Screen screenWas = screen_;
    hover_ = none;
    int pressed = none;
    bool back = false;
    for (unsigned i = 0; i < 2; ++i) {
        const Hand& h = in.hands[i];
        laserOn_[i] = false;
        if (!h.b)
            bReleased_[i] = true;
        else if (bReleased_[i]) {
            bReleased_[i] = false;
            back = back || armed;
        }
        const bool aEdge = h.a && aReleased_[i];
        if (!h.a)
            aReleased_[i] = true;
        else
            aReleased_[i] = false;
        if (!h.tracked || !okPose(h.grip) || !okPose(h.aim)) {
            released_[i] = false;
            continue;
        }
        // Poke: the hand within pokeDepth of the face over a button.
        const Vec3 local = panel_.orientation.conjugate().rotate(h.grip.position - panel_.position);
        const Button* poked = finite(local) && std::abs(local.z) <= pokeDepth ? find(bs, n, local, pokeSlack) : nullptr;
        if (poked) {
            hover_ = poked->id;
            if (armed && !wasNear_[i] && poked->enabled)
                pressed = poked->id;
        }
        wasNear_[i] = poked != nullptr;
        // Aim: the laser on the panel, its button, a fresh pull (or A) presses it.
        Vec3 hit{};
        const Vec3 way = h.aim.orientation.rotate({0, 0, -1});
        const bool onPlane = hitPlane(panel_, h.aim.position, way, hit);
        const Button* aimed = onPlane ? find(bs, n, hit, aimSlack) : nullptr;
        // 0.0.73 fix 1: the button already lit stays lit while the laser is still within stickySlack
        // of it, before any neighbour (a shaking hand on a button's edge, or on the gap between two
        // tiles, no longer flips it, its frame and the help line).
        if (onPlane && hoverWas != none && (!aimed || aimed->id != hoverWas))
            for (uint32_t k = 0; k < n; ++k)
                if (bs[k].id == hoverWas && find(&bs[k], 1, hit, stickySlack))
                    aimed = &bs[k];
        if (aimed && hover_ == none)
            hover_ = aimed->id;
        const float edge = laserWas_[i] ? .16f : .1f; // the laser too: on at .1 past the edge, off past .16
        if (onPlane && std::abs(hit.x) <= W + edge && std::abs(hit.y) <= H + edge) {
            laserOn_[i] = true;
            laserFrom_[i] = h.aim.position;
            laserTo_[i] = panel_.position + panel_.orientation.rotate({hit.x, hit.y, .008f});
        }
        laserWas_[i] = laserOn_[i];
        bool pull = false;
        if (h.trigger < triggerOff)
            released_[i] = true;
        else if (h.trigger > triggerOn && released_[i]) {
            released_[i] = false;
            pull = true;
        }
        if (armed && aimed && aimed->enabled && (pull || aEdge) && pressed == none)
            pressed = aimed->id;
    }
    // 0.0.73 fix 1: crossing a gap between two tiles (or a dropped frame of tracking) keeps the last
    // lit button for hoverHoldSeconds rather than blinking everything off for a frame or two.
    if (hover_ != none)
        hoverSeen_ = in.seconds;
    else if (hoverWas != none && screen_ == screenWas && in.seconds - hoverSeen_ < hoverHoldSeconds)
        hover_ = hoverWas;
    if (pressed != none) {
        ++presses_;
        if (pressed == closeButton) {
            close();
            act.closed = true;
            return act;
        }
        if (pressed == suitsTab && screen_ == Screen::controls) {
            screen_ = Screen::hero;
        } else if (pressed == controlsTab && screen_ == Screen::hero) {
            screen_ = Screen::controls;
        } else if (screen_ == Screen::grid && pressed >= tile && pressed < tile + static_cast<int>(heroes)) {
            screen_ = Screen::hero;
            hero_ = static_cast<uint32_t>(pressed - tile);
        } else if (pressed == backButton) {
            screen_ = Screen::grid;
        } else if (pressed == playButton) {
            act.playAs = static_cast<int>(hero_);
        } else if (pressed >= row && screen_ == Screen::hero) {
            act.suitHero = static_cast<int>(hero_);
            act.suitIndex = pressed - row;
        }
        hover_ = none;
    } else if (back) {
        if (screen_ == Screen::controls)
            screen_ = Screen::hero, hover_ = none;
        else if (screen_ == Screen::hero)
            screen_ = Screen::grid, hover_ = none;
        else {
            close();
            act.closed = true;
            return act;
        }
    }
    // Walked (or swung) well away from it: it closes.
    if (okPose(in.head) && length(in.head.position - panel_.position) > 4.f) {
        close();
        act.closed = true;
    }
    return act;
}

View view(const Page& p, uint32_t playing, Pose worldFromTracking) {
    View v;
    if (!p.isOpen() || !okPose(worldFromTracking))
        return v;
    v.panel = compose(worldFromTracking, p.panel());
    v.opacity = okPose(v.panel) ? p.opacity() : 0.f;
    v.screen = p.screen();
    v.hero = p.hero();
    v.playing = playing;
    v.hover = p.hover();
    v.version = version();
    for (unsigned i = 0; i < 2; ++i) {
        Vec3 a{}, b{};
        if (v.opacity > 0 && p.laser(i, a, b)) {
            v.laserFrom[i] = worldFromTracking.position + worldFromTracking.orientation.rotate(a);
            v.laserTo[i] = worldFromTracking.position + worldFromTracking.orientation.rotate(b);
            v.laser[i] = finite(v.laserFrom[i]) && finite(v.laserTo[i]);
        }
    }
    return v;
}

bool pictures(const mesh_model::Mesh* m, Picture (&out)[pictureCount]) {
    for (auto& p : out)
        p = Picture{};
    if (!m || m->triangles() != 2 * pictureCount || m->image.size() < 2 * pictureCount || m->uvs.size() < 12 * pictureCount)
        return false;
    for (uint32_t k = 0; k < pictureCount; ++k) {
        const int16_t im = m->image[2 * k];
        if (im < 0 || static_cast<size_t>(im) >= m->images.size() || m->images[static_cast<size_t>(im)].layer < 0)
            continue;
        float u0 = 1e9f, v0 = 1e9f, u1 = -1e9f, v1 = -1e9f;
        for (uint32_t c = 0; c < 6; ++c) {
            const float u = m->uvs[2 * k * 6 + c * 2], v = m->uvs[2 * k * 6 + c * 2 + 1];
            u0 = std::min(u0, u), u1 = std::max(u1, u), v0 = std::min(v0, v), v1 = std::max(v1, v);
        }
        if (!(u1 > u0 && v1 > v0 && u0 >= -.01f && u1 <= 1.01f && v0 >= -.01f && v1 <= 1.01f))
            continue;
        out[k] = Picture{m->images[static_cast<size_t>(im)].layer, u0, v0, u1, v1};
    }
    return true;
}

namespace {
const Vec3 gold{1.f, .62f, .08f}, plateColour{.012f, .016f, .03f}, ink{.92f, .92f, .95f}, dim{.62f, .66f, .74f};
Vec3 statusColour(Status s) {
    switch (s) {
    case Status::inUse: return {.35f, 1.f, .5f};
    case Status::unlocked: return ink;
    case Status::lockedFinal:
    case Status::lockedRep: return gold;
    case Status::missing: return {1.f, .32f, .26f};
    }
    return ink;
}
std::string statusText(const Row& r) {
    switch (r.status) {
    case Status::inUse: return "IN USE";
    case Status::unlocked: return "UNLOCKED";
    case Status::lockedFinal: return "LOCKED: MAX REP (FINAL)";
    case Status::lockedRep: {
        char t[48];
        std::snprintf(t, sizeof t, "LOCKED: REP %d", static_cast<int>(r.rep));
        return t;
    }
    case Status::missing: return r.final ? "MODEL MISSING - FINAL: MAX REP" : "MODEL MISSING";
    }
    return "";
}
struct Builder {
    std::vector<Vertex>& out;
    std::vector<Vertex>& images;
    const Picture* pics;
    bool textured;
    void quad(float x0, float y0, float x1, float y1, float z, Vec3 c, float a) {
        for (const Vec3& p : {Vec3{x0, y0, z}, Vec3{x1, y0, z}, Vec3{x1, y1, z}, Vec3{x0, y0, z}, Vec3{x1, y1, z}, Vec3{x0, y1, z}})
            out.push_back(Vertex{p, c, a});
    }
    void frame(float x0, float y0, float x1, float y1, float e, float z, Vec3 c, float a) {
        quad(x0 - e, y1, x1 + e, y1 + e, z, c, a);
        quad(x0 - e, y0 - e, x1 + e, y0, z, c, a);
        quad(x0 - e, y0, x0, y1, z, c, a);
        quad(x1, y0, x1 + e, y1, z, c, a);
    }
    // Text from (x, y) on its baseline; shrunk (to 70 %) then cut to fit `most` wide.
    void text(std::string t, float x, float y, float h, Vec3 c, float most, bool centre = false, float weight = .12f) {
        float fh = fittedHeight(t, h, most);
        if (fh < h * .7f) {
            fh = h * .7f;
            while (t.size() > 3 && textWidth(t + "..", fh) > most)
                t.pop_back();
            t += "..";
        }
        TextStyle ts;
        ts.height = fh;
        ts.color = c;
        ts.soft = fh >= .02f ? .0012f : 0.f; // soft edges on the big lines only: a third of the vertices
        ts.weight = weight;
        const float w = textWidth(t, fh);
        appendText(out, t, Vec3{centre ? x - w / 2 : x, y, .003f}, Vec3{1, 0, 0}, Vec3{0, 1, 0}, ts);
    }
    // Wrapped lines from the top baseline y; returns the next free baseline.
    float para(const std::string& t, float x, float y, float h, Vec3 c, float most, uint32_t maxLines, float step) {
        const auto ls = test_notice::lines(t, h, most);
        uint32_t k = 0;
        for (const auto& l : ls) {
            if (k++ >= maxLines)
                break;
            text(std::string(l), x, y, h, c, most);
            y -= step;
        }
        return y;
    }
    // A picture (textured, unlit; dimmed by `light`), or PICTURE MISSING in its place.
    void picture(int k, float x0, float y0, float x1, float y1, float light = 1.f) {
        const Picture* p = k >= 0 && k < static_cast<int>(pictureCount) ? &pics[k] : nullptr;
        if (textured && p && p->layer >= 0) {
            const float L = static_cast<float>(p->layer);
            const Vec3 c{light, light, light};
            const Vertex tl{{x0, y1, .004f}, c, 1.f, {p->u0, p->v0, L, 1}}, bl{{x0, y0, .004f}, c, 1.f, {p->u0, p->v1, L, 1}},
                br{{x1, y0, .004f}, c, 1.f, {p->u1, p->v1, L, 1}}, tr{{x1, y1, .004f}, c, 1.f, {p->u1, p->v0, L, 1}};
            for (const Vertex& v : {tl, bl, br, tl, br, tr})
                images.push_back(v);
            return;
        }
        quad(x0, y0, x1, y1, .002f, Vec3{.08f, .09f, .12f}, .95f);
        const float s = std::min(x1 - x0, y1 - y0);
        text("PICTURE", (x0 + x1) / 2, (y0 + y1) / 2 + s * .04f, s * .11f, dim, (x1 - x0) * .9f, true);
        text("MISSING", (x0 + x1) / 2, (y0 + y1) / 2 - s * .14f, s * .11f, dim, (x1 - x0) * .9f, true);
    }
    void button(const Button& b, const char* label, bool hover, bool on) {
        const Vec3 face = !on ? Vec3{.25f, .25f, .28f} : hover ? Vec3{1.f, .86f, .38f} : gold * .8f;
        if (hover)
            frame(b.x0, b.y0, b.x1, b.y1, .006f, .0025f, Vec3{1, 1, 1}, 1.f);
        quad(b.x0, b.y0, b.x1, b.y1, .002f, face, .97f);
        const float h = std::min(.034f, (b.y1 - b.y0) * .45f);
        text(label, (b.x0 + b.x1) / 2, (b.y0 + b.y1) / 2 - h / 2, h, on ? Vec3{.02f, .02f, .03f} : dim, (b.x1 - b.x0) - .02f, true, .16f);
    }
};

void build(Cache& c, const View& v, const Snapshot* s, const mesh_model::Mesh* roster, bool textured) {
    c.plate.clear();
    c.images.clear();
    ++c.rebuilds;
    Picture pics[pictureCount];
    pictures(roster, pics);
    Builder b{c.plate, c.images, pics, textured};
    const float rim = .008f;
    b.quad(-W, -H, W, H, 0, plateColour, .93f);
    b.frame(-W, -H, W, H, rim, .001f, gold, .95f);
    Button bs[buttonsMost];
    const uint32_t n = buttons(v.screen, v.hero, s, bs);
    const auto byId = [&](int id) -> const Button* {
        for (uint32_t i = 0; i < n; ++i)
            if (bs[i].id == id)
                return &bs[i];
        return nullptr;
    };
    if (const Button* x = byId(closeButton))
        b.button(*x, "CLOSE", v.hover == closeButton, true);
    if (v.screen == Screen::grid) {
        b.text("HERO ROSTER", -.58f, .355f, .045f, gold, .9f, false, .15f);
        for (uint32_t k = 0; k < heroes; ++k) {
            const Button* t = byId(tile + static_cast<int>(k));
            if (!t)
                continue;
            const bool hov = v.hover == tile + static_cast<int>(k), now = v.playing == k;
            b.quad(t->x0, t->y0, t->x1, t->y1, .001f, hov ? Vec3{.12f, .16f, .26f} : Vec3{.04f, .05f, .09f}, .92f);
            if (now)
                b.frame(t->x0, t->y0, t->x1, t->y1, .006f, .002f, gold, 1.f);
            if (hov)
                b.frame(t->x0, t->y0, t->x1, t->y1, .006f, .0025f, Vec3{1, 1, 1}, 1.f);
            const float cx = (t->x0 + t->x1) / 2, py1 = t->y1 - .012f, py0 = py1 - tilePic;
            b.picture(static_cast<int>(k), cx - tilePic / 2, py0, cx + tilePic / 2, py1);
            b.text(shortName(k), cx, py0 - .034f, .024f, now ? gold : ink, t->x1 - t->x0 - .02f, true, .14f);
        }
        b.text(helpFor(v.screen, v.hero, v.hover, s), -.58f, -.437f, .019f, dim, 1.16f);
    } else {
        const uint32_t h = std::min(v.hero, heroes - 1);
        const hero072::Bio& bio = hero072::bio(h);
        b.picture(static_cast<int>(h), portX0, portY0, portX1, portY1);
        b.text(bio.name, portX0, portY0 - .052f, .04f, gold, portX1 - portX0, false, .15f);
        b.text(std::string("REAL NAME: ") + bio.identity, portX0, portY0 - .092f, .02f, ink, portX1 - portX0);
        float y = b.para(bio.abilities, portX0, portY0 - .128f, .018f, Vec3{.55f, .85f, 1.f}, portX1 - portX0, 2, .026f);
        b.para(bio.bio, portX0, y - .006f, .018f, dim, portX1 - portX0, 4, .026f);
        if (const Button* x = byId(playButton)) {
            const bool now = v.playing == h;
            b.button(*x, now ? "PLAYING NOW" : (std::string("PLAY AS ") + bio.name).c_str(), v.hover == playButton, !now);
        }
        if (const Button* x = byId(backButton))
            b.button(*x, "BACK", v.hover == backButton, true);
        // 0.0.73 test 2 (fix 23): the SUITS and CONTROLS tabs; the open one is lit gold, the other a button.
        const bool guide = v.screen == Screen::controls;
        const auto tab = [&](int id, const char* label, bool open) {
            const Button* x = byId(id);
            if (!x)
                return;
            if (open) {
                b.quad(x->x0, x->y0, x->x1, x->y1, .002f, gold * .85f, .97f);
                b.text(label, (x->x0 + x->x1) / 2, (x->y0 + x->y1) / 2 - .014f, .028f, Vec3{.02f, .02f, .03f},
                       x->x1 - x->x0 - .02f, true, .16f);
            } else {
                const bool hov = v.hover == id;
                b.quad(x->x0, x->y0, x->x1, x->y1, .002f, hov ? Vec3{.16f, .2f, .32f} : Vec3{.06f, .08f, .14f}, .95f);
                b.frame(x->x0, x->y0, x->x1, x->y1, .004f, .0025f, hov ? Vec3{1, 1, 1} : gold * .7f, 1.f);
                b.text(label, (x->x0 + x->x1) / 2, (x->y0 + x->y1) / 2 - .014f, .028f, hov ? Vec3{1, 1, 1} : ink,
                       x->x1 - x->x0 - .02f, true, .16f);
            }
        };
        tab(suitsTab, "SUITS", !guide);
        tab(controlsTab, "CONTROLS", guide);
        if (guide) {
            const hero_controls::Guide& g = hero_controls::guideOf(h);
            const uint32_t lines = std::min(g.count, hero_controls::linesMost);
            const float lh = std::min(.062f, (rowsTop - rowsBottom) / static_cast<float>(std::max(1u, lines)));
            for (uint32_t i = 0; i < lines; ++i) {
                const float top = rowsTop - static_cast<float>(i) * lh, y0 = top - lh + .006f;
                b.quad(rowsX0, y0, rowsX1, top, .001f, i % 2 ? Vec3{.04f, .05f, .09f} : Vec3{.06f, .08f, .13f}, .9f);
                const float ty = (y0 + top) / 2 - .011f;
                b.text(g.lines[i].name, rowsX0 + .015f, ty, .022f, gold, .25f, false, .14f);
                b.text(g.lines[i].how, rowsX0 + .28f, ty, .02f, ink, rowsX1 - rowsX0 - .295f);
            }
        } else if (!s)
            b.text("READING THE SUITS...", rowsX0, .25f, .024f, dim, rowsX1 - rowsX0);
        else {
            const uint32_t rows = std::min(suitsMost, s->count[h]);
            const float rh = rowHeight(rows), k = rh / .095f;
            for (uint32_t i = 0; i < rows; ++i) {
                const Button* x = byId(row + static_cast<int>(i));
                if (!x)
                    continue;
                const Row& r = s->rows[h][i];
                const bool hov = v.hover == row + static_cast<int>(i);
                b.quad(x->x0, x->y0, x->x1, x->y1, .001f, hov ? Vec3{.12f, .16f, .26f} : Vec3{.05f, .07f, .12f}, .9f);
                if (hov)
                    b.frame(x->x0, x->y0, x->x1, x->y1, .005f, .0025f, Vec3{1, 1, 1}, 1.f);
                if (r.status == Status::inUse)
                    b.quad(x->x0, x->y0, x->x0 + .006f, x->y1, .002f, statusColour(Status::inUse), 1.f);
                const float th = (x->y1 - x->y0) - .014f, tx0 = x->x0 + .012f, ty0 = x->y0 + .007f;
                int pic = i == 0 ? (h == hero_modes::captainAmerica ? pictureOf("CapSuit") : static_cast<int>(h)) : pictureOf(r.folder);
                b.picture(pic, tx0, ty0, tx0 + th, ty0 + th, r.status == Status::missing || r.status == Status::lockedFinal ||
                                                                     r.status == Status::lockedRep
                                                                 ? .55f
                                                                 : 1.f);
                const float tx = tx0 + th + .016f, most = x->x1 - tx - .01f;
                float ty = x->y1 - .01f - .024f * k;
                if (i == 0) {
                    b.text("USE MY OWN MOD OR IN-GAME SUIT", tx, ty, .024f * k, ink, most, false, .14f);
                    ty -= .027f * k;
                    b.text(h == hero_modes::captainAmerica ? "OR THE CAPSUIT SWAP BY IQBALMULYAWAN (CC BY), ON YOUR PC"
                                                           : "THE GAME'S OWN SUIT, OR THE ONE YOUR SUIT MOD PUTS ON",
                           tx, ty, .016f * k, dim, most);
                } else {
                    b.text(std::string(r.name) + (r.final ? " - FINAL" : ""), tx, ty, .024f * k, ink, most, false, .14f);
                    ty -= .027f * k;
                    b.text(std::string("BY ") + r.artist + (r.licence[0] ? std::string(" (") + r.licence + ")" : ""), tx, ty,
                           .016f * k, dim, most);
                }
                ty -= .024f * k;
                b.text(i == 0 && s->ownSuit ? "IN USE (OWN SUIT IS ON)" : statusText(r), tx, ty, .016f * k, statusColour(r.status),
                       most, false, .14f);
            }
        }
        b.para(helpFor(v.screen, h, v.hover, s), rowsX0, -.29f, .019f, dim, rowsX1 - rowsX0, 4, .027f);
    }
}
} // namespace

void append(std::vector<Vertex>& out, std::vector<Vertex>* models, const View& v, Cache& c, const mesh_model::Mesh* roster,
            const Snapshot* s) {
    if (!(v.opacity > 0) || !okPose(v.panel) || v.screen == Screen::closed)
        return;
    const Snapshot* use = s;
    uint32_t version = v.version;
    if (!use) {
        if (version && (c.key == ~0ull || static_cast<uint32_t>(c.key >> 32) != version))
            version = latest(c.snap);
        use = version ? &c.snap : nullptr;
    }
    const bool textured = models != nullptr && roster != nullptr;
    const uint64_t key = (static_cast<uint64_t>(version) << 32) | (static_cast<uint64_t>(v.screen) << 28) |
                         (static_cast<uint64_t>(v.hero & 15u) << 24) | (static_cast<uint64_t>(v.playing & 15u) << 20) |
                         (static_cast<uint64_t>((v.hover + 1) & 255) << 8) | (textured ? 1u : 0u) | (s ? 2u : 0u);
    if (key != c.key) {
        build(c, v, use, roster, textured);
        c.key = key;
    }
    const size_t from = out.size(), modelsFrom = models ? models->size() : 0;
    const Vec3 r = v.panel.orientation.rotate({1, 0, 0}), u = v.panel.orientation.rotate({0, 1, 0}),
               f = v.panel.orientation.rotate({0, 0, 1}), o = v.panel.position;
    const float a = std::clamp(v.opacity, 0.f, 1.f);
    out.reserve(out.size() + c.plate.size() + 64);
    for (const Vertex& x : c.plate) {
        Vertex y = x;
        y.position = o + r * x.position.x + u * x.position.y + f * x.position.z;
        y.alpha *= a;
        out.push_back(y);
    }
    if (models && a > .55f) {
        models->reserve(models->size() + c.images.size());
        for (const Vertex& x : c.images) {
            Vertex y = x;
            y.position = o + r * x.position.x + u * x.position.y + f * x.position.z;
            models->push_back(y);
        }
    }
    // The lasers: a bright beam (two crossed strips) from the hand to the panel, and a dot there.
    const auto quadW = [&](float x0, float y0, float x1, float y1, float z, Vec3 col, float al) {
        for (const Vec3& p : {Vec3{x0, y0, z}, Vec3{x1, y0, z}, Vec3{x1, y1, z}, Vec3{x0, y0, z}, Vec3{x1, y1, z}, Vec3{x0, y1, z}})
            out.push_back(Vertex{o + r * p.x + u * p.y + f * p.z, col, al});
    };
    for (unsigned k = 0; k < 2; ++k) {
        if (!v.laser[k])
            continue;
        const Vec3 s0 = v.laserFrom[k], s1 = v.laserTo[k], d = s1 - s0;
        const float len = length(d);
        if (!(len > .02f && len < 20.f))
            continue;
        const Vec3 dir = d * (1.f / len);
        Vec3 side = cross(dir, u);
        if (!(length(side) > .1f))
            side = cross(dir, r);
        side = normalized(side);
        const Vec3 up2 = normalized(cross(side, dir));
        for (const Vec3& across : {side, up2}) {
            const Vec3 h0 = across * (test_notice::laserWidth / 2);
            for (const Vec3& p : {s0 - h0, s0 + h0, s1 + h0, s0 - h0, s1 + h0, s1 - h0})
                out.push_back(Vertex{p, Vec3{.55f, .9f, 1.f}, .85f * a});
        }
        const Vec3 cpt = v.panel.orientation.conjugate().rotate(s1 - o);
        const float dot2 = test_notice::laserDot / 2;
        quadW(cpt.x - dot2, cpt.y - dot2, cpt.x + dot2, cpt.y + dot2, .009f, Vec3{1, 1, 1}, a);
    }
    for (size_t i = from; i < out.size(); ++i)
        if (!finite(out[i].position) || !std::isfinite(out[i].alpha)) {
            out.resize(from);
            if (models)
                models->resize(modelsFrom);
            return;
        }
}
} // namespace spidy::roster_page
