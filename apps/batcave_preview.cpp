// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// 0.0.74 test 2b BATCAVE PREVIEW: the Batcave's chair, the Batcomputer's pages and the dash's CRIME GPS, drawn
// without a headset by the overlay's own vertices (batcave074::append, van_screen::append - the code the game
// runs) - SIMULATED, NOT GAME CAPTURE. The room (scene.glb) is drawn untextured in its base colours, lit flat.
// Into <folder>:
//   chair_walkup.bmp     the chair and the screen from standing, 2 m behind
//   chair_seated.bmp     from the seated eye, the right hand's laser on a row
//   page_<name>.bmp      each Batcomputer page from the seated eye (laser on, crimes fed)
//   dash_crime_popup_<car>.bmp  the dash MAP with "CRIME IN PROGRESS - press OK to track"
//   dash_crime_tracked_<car>.bmp the MAP with the tracked crime and its arrow
//
//   spidy_batcave_preview <folder> [<Batcave folder with scene.glb>]
#include "spidy/batcave074.hpp"
#include "spidy/crime_gps074.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/van072.hpp"
#include "spidy/van_screen.hpp"
#include "preview_image.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>
using namespace spidy;
using namespace spidy::preview;
namespace bc = spidy::batcave074;
namespace cg = spidy::crime_gps074;
namespace vs = spidy::van_screen;

namespace {
void fill(Image& image, Vec3 top, Vec3 bottom) {
    for (int y = 0; y < image.h; ++y)
        for (int x = 0; x < image.w; ++x) {
            const float v = static_cast<float>(y) / static_cast<float>(image.h);
            float* o = image.at(x, y);
            o[0] = decode(top.x + (bottom.x - top.x) * v);
            o[1] = decode(top.y + (bottom.y - top.y) * v);
            o[2] = decode(top.z + (bottom.z - top.z) * v);
        }
}
std::vector<Vertex> toCamera(const std::vector<Vertex>& world, Vec3 eye, Vec3 right, Vec3 up, Vec3 back) {
    auto out = world;
    for (auto& v : out) {
        const Vec3 d = v.position - eye;
        v.position = {dot(d, right), dot(d, up), dot(d, back)};
    }
    return out;
}
std::vector<Vertex> roomVerts;
void loadRoom(const std::string& folder) {
    mesh_model::Mesh m;
    std::string err;
    if (mesh_model::loadFolder(folder + "/", true, m, err, mesh_model::hideoutTriangles) != mesh_model::loaded) {
        std::printf("room: not loaded (%s)\n", err.c_str());
        return;
    }
    const Vec3 light = normalized(Vec3{.3f, .9f, .4f});
    for (uint32_t t = 0; t < m.triangles(); ++t) {
        const Vec3 a = m.corners[t * 3], b = m.corners[t * 3 + 1], c = m.corners[t * 3 + 2];
        const Vec3 n = normalized(cross(b - a, c - a));
        const float l = .25f + .45f * std::fabs(dot(n, light));
        const Vec3 col = m.colors[t] * (l * .55f);
        for (Vec3 p : {a, b, c})
            roomVerts.push_back({p, col, 1.f, {}});
    }
    std::printf("room: %u triangles\n", m.triangles());
}
// A design point (x right, y up) on the panel in room space (the panel faces +z, centred on screenCentre).
Vec3 panelPoint(float x, float y) { return bc::screenCentre + Vec3{x * bc::screenScale, y * bc::screenScale, 0}; }
float rowY(uint32_t row) {
    for (float y = bc::designHeight / 2; y > -bc::designHeight / 2; y -= .002f)
        if (bc::rowAt(-.3f, y) == static_cast<int>(row))
            return y - bc::rowStep * .4f;
    return 0;
}
bool shot(const std::string& path, const bc::View& v, Vec3 eye, Vec3 target, float focal) {
    Image image(1280, 960);
    fill(image, {.03f, .035f, .045f}, {.01f, .01f, .012f});
    const Vec3 back = normalized(eye - target), right = normalized(cross(Vec3{0, 1, 0}, back)), up = cross(back, right);
    const Camera cam{focal, 640.f, 480.f};
    std::vector<float> depth(static_cast<size_t>(image.w) * image.h, std::numeric_limits<float>::infinity());
    std::vector<Vertex> solid, glow;
    const bool good = bc::append(solid, glow, v, eye);
    std::vector<Vertex> all = roomVerts;
    all.insert(all.end(), solid.begin(), solid.end());
    draw(image, toCamera(all, eye, right, up, back), cam, 0, 0, 0, 0, image.w, image.h, &depth);
    draw(image, toCamera(glow, eye, right, up, back), cam, 0, 0, 0, 0, image.w, image.h, &depth);
    std::printf("%s: %s, solid %zu glow %zu, hotRow %d\n", path.c_str(), good ? "drawn" : "NOT DRAWN", solid.size() / 3,
                glow.size() / 3, v.hotRow);
    return save(image, path) && good;
}
cg::Tracker tracker;
void feedCrimes() {
    const Vec3 player{37.f, 0, -112.f};
    // two groups at t 0..2; the second leaves at t 3 with the player far: LOST; the first tracked
    const Vec3 a[] = {{37.f + 120, 0, -112.f - 160}, {37.f + 124, 0, -112.f - 158}, {37.f + 118, 0, -112.f - 163},
                      {37.f - 210, 0, -112.f + 60}, {37.f - 214, 0, -112.f + 62}};
    for (int k = 0; k <= 4; ++k)
        tracker.step(static_cast<float>(k) * .5f, a, 5, player, true);
    tracker.accept(2.f);
    for (int k = 5; k <= 16; ++k)
        tracker.step(static_cast<float>(k) * .5f, a, 3, player, true);
    const Vec3 b[] = {a[0], a[1], a[2], {37.f + 40, 0, -112.f + 300}, {37.f + 43, 0, -112.f + 302}};
    for (int k = 17; k <= 20; ++k)
        tracker.step(static_cast<float>(k) * .5f, b, 5, player, true);
}
bc::Feed feed() {
    bc::Feed fd;
    fd.crimes = &tracker;
    fd.city = {37.f, 0, -112.f};
    fd.cityOk = true;
    fd.now = 10.f;
    fd.car = "1989";
    fd.song = "Example Artist - Example Song";
    fd.musicOn = true;
    fd.musicPlaying = true;
    return fd;
}
// The computer on `page` (tapped from the menu), the right hand's laser on `row`.
bc::View computerView(uint32_t page, uint32_t row) {
    bc::Computer c;
    c.open();
    const uint32_t live = tracker.liveCount();
    if (page != bc::menu)
        c.tap(-.3f, rowY(page - 1), live);
    const Vec3 eye = bc::chairFeet + Vec3{0, bc::seatedEye, 0};
    bc::In in;
    in.liveCrimes = live;
    in.tracked[1] = true;
    in.hand[1] = eye + Vec3{.22f, -.38f, -.18f};
    in.aim[1] = normalized(panelPoint(-.3f, rowY(row)) - in.hand[1]);
    in.grip[1] = .9f;
    c.step(in);
    bc::View v;
    v.on = true;
    v.batcave = true;
    v.computer = true;
    const bc::Feed fd = feed();
    v.page = c.page();
    v.cursor = c.cursor();
    v.lines = bc::pageLines(v.page, v.cursor, fd);
    v.cursorOn = c.cursorOn();
    v.cursorAt = c.cursorAt();
    v.beamOn = c.beamOn();
    v.beamFrom = c.beamFrom();
    v.hotRow = c.hotRow();
    v.hotButton = static_cast<int>(c.hotButton());
    if (v.page == bc::map)
        v.mapDots = bc::mapDots(fd);
    if (v.page != page)
        std::printf("page %u: opened %u instead\n", page, v.page);
    return v;
}
bool dash(const std::string& path, bool batman, bool popup) {
    van072::Frame f;
    f.at = {37.f, 0, -112.f};
    f.yaw = -.79f;
    vs::Screen s;
    while (s.page() != vs::map)
        s.next();
    vs::View v = vs::view(f, s, 10.4f, 16.f, .55f, true);
    v.flash = 0;
    v.offer.batman = batman;
    v.offer.crimePopup = popup;
    if (batman) {
        v.carName = "1989";
    }
    v.crimeOn = true;
    v.crimes = 0;
    for (const auto& c : tracker.live())
        if (c.id && v.crimes < vs::View::mostCrimes) {
            v.crimeX[v.crimes] = c.at.x;
            v.crimeZ[v.crimes] = c.at.z;
            if (!popup && c.id == tracker.tracked()) {
                v.crimeTracked = static_cast<int>(v.crimes);
                cg::bearingTo(f.at, c.at, v.crimeBearing, v.crimeMetres);
            }
            ++v.crimes;
        }
    v.crimePopup = popup;
    v.crimePopupLeft = popup ? 9.f : 0.f;
    Image image(1120, 680);
    fill(image, {.02f, .022f, .02f}, {.01f, .01f, .01f});
    std::vector<Vertex> w;
    const Vec3 r = normalized(v.right), u = normalized(v.up), n = cross(r, u);
    const Vec3 eye = v.centre + n * (.40f * v.scale);
    const bool good = vs::append(w, v, eye);
    draw(image, toCamera(w, eye, r, u, n), Camera{1300.f, 560.f, 340.f}, 0, 0, 0, 0, image.w, image.h);
    std::printf("%s: %s crimes %u tracked %d %.0f m %.0f deg\n", path.c_str(), good ? "drawn" : "NOT DRAWN", v.crimes,
                v.crimeTracked, v.crimeMetres, v.crimeBearing);
    return save(image, path) && good;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argv[1][0] == '-') {
        std::fprintf(stderr, "spidy_batcave_preview <folder> [<Batcave folder with scene.glb>]\n");
        return 2;
    }
    const std::string dir = std::string(argv[1]) + "/";
    if (argc > 2)
        loadRoom(argv[2]);
    feedCrimes();
    std::printf("crimes: live %u past %u log %u tracked %u\n", tracker.liveCount(), tracker.pastCount(), tracker.logCount(),
                tracker.tracked());
    bool good = true;
    const Vec3 seated = bc::chairFeet + Vec3{0, bc::seatedEye, 0};
    {   // walking up: the chair, the panel off (not sat) and the computer's marker
        bc::View v;
        v.on = true;
        v.batcave = true;
        v.marks = 1;
        v.markAt[0] = {-15.7f, .32f, -1.4f};
        v.markKind[0] = 1;
        v.markLit = 0;
        good &= shot(dir + "chair_walkup.bmp", v, bc::chairFeet + Vec3{.9f, 1.65f, 2.4f}, bc::chairFeet + Vec3{0, .6f, -.6f}, 760.f);
    }
    good &= shot(dir + "chair_seated.bmp", computerView(bc::menu, 1), seated + Vec3{.35f, .25f, .55f}, bc::screenCentre, 700.f);
    for (uint32_t p = 0; p < bc::pages; ++p) {
        if (p == bc::returnCity)
            continue;
        std::string name = bc::pageNames[p];
        for (auto& ch : name)
            ch = ch == ' ' ? '_' : static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        good &= shot(dir + "page_" + name + ".bmp", computerView(p, 1), seated, bc::screenCentre, 470.f);
    }
    good &= dash(dir + "dash_crime_popup_batmobile.bmp", true, true);
    good &= dash(dir + "dash_crime_tracked_batmobile.bmp", true, false);
    good &= dash(dir + "dash_crime_popup_van.bmp", false, true);
    good &= dash(dir + "dash_crime_tracked_van.bmp", false, false);
    std::printf(good ? "ALL DRAWN\n" : "SOME NOT DRAWN\n");
    return good ? 0 : 1;
}
