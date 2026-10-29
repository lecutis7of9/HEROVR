// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.73 test 3 VAN SCREEN: the dash screen's pages, drawn without a headset by the overlay's own
// vertices (van_screen::append, the code the game runs) - SIMULATED, NOT GAME CAPTURE. Into <folder>:
//   page_map.bmp page_status.bmp page_cases_N.bmp (N 1..7) page_locker.bmp  each page face-on
//   page_music.bmp page_music_empty.bmp page_home.bmp page_home_sat.bmp  (0.0.74 test 1)
//   driver_seat.bmp  from the driver's eye (van072::seatEye) in the built cab (when <models> has the van's
//                    Vehicle/PunisherVan/Interior and .../SteeringWheel; untextured, darkened), the screen right of the wheel
//
//   spidy_van_screen_preview <folder> [<models folder>]
#include "spidy/hero007_visual.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/van072.hpp"
#include "spidy/van_screen.hpp"
#include "preview_image.hpp"

#include <cmath>
#include <limits>
#include <cstdio>
#include <string>
#include <vector>
using namespace spidy;
using namespace spidy::preview;
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
vs::View pageView(uint32_t page, uint32_t file, float speed, float yaw, float rpm) {
    van072::Frame f;
    f.at = {37.f, 0, -112.f};
    f.yaw = yaw;
    vs::Screen s;
    while (s.page() != page || s.file() != file)
        s.next();
    vs::View v = vs::view(f, s, speed, 16.f, rpm, true);
    v.flash = 0;
    return v;
}
bool page(const std::string& path, const vs::View& v) {
    Image image(1120, 680);
    fill(image, {.02f, .022f, .02f}, {.01f, .01f, .01f});
    std::vector<Vertex> w;
    const Vec3 r = normalized(v.right), u = normalized(v.up), n = cross(r, u);
    const Vec3 eye = v.centre + n * (.40f * v.scale); // the art fills the frame whatever its size on the dash
    if (!vs::append(w, v, eye))
        return false;
    const Camera cam{1300.f, 560.f, 340.f};
    draw(image, toCamera(w, eye, r, u, n), cam, 0, 0, 0, 0, image.w, image.h);
    return save(image, path);
}
// 0.0.74 test 2 DASH POINTER: the screen after Screen::step with the pointer on (the game's own code): the
// right hand at `from` (car local) aimed at tab `tab` (or, `touch`, its fingertip on it), the bat palette when
// `batman`. The page stays as it was until the trigger clicks.
vs::View pointerView(uint32_t page, uint32_t tab, bool batman, bool touch, float yaw) {
    van072::Frame f;
    f.at = {37.f, 0, -112.f};
    f.yaw = yaw;
    vs::Screen s;
    while (s.page() != page)
        s.next();
    vs::Offer offer;
    offer.batman = batman;
    const vs::Mount m = vs::mountLocal();
    const float tabY = vs::height / 2 - vs::tabHeight / 2;
    const Vec3 target = m.centre + (m.right * vs::tabCentre(tab) + m.up * tabY) * vs::faceScale;
    vs::In i;
    i.dt = 1.f / 90;
    i.driving = true;
    i.pointer = true;
    i.offer = offer;
    i.tracked[1] = true;
    i.grip[1] = .9f;
    i.trigger[1] = 0;
    if (touch) {
        i.aim[1] = m.normal * -1.f;
        i.hand[1] = target + m.normal * (vs::fingerReach + .02f); // 2 cm short: hovering, not yet pressed
    } else {
        const Vec3 from{-.12f, 1.0f, -.75f};
        i.hand[1] = from;
        i.aim[1] = normalized(target - from);
    }
    s.step(i);
    vs::View v = vs::view(f, s, 10.4f, 16.f, .55f, true);
    v.flash = 0;
    if (batman) {
        v.offer.batman = true;
        v.carName = "1989";
        v.carCredit = "MODEL: BAT-MOBILE (1989) BY DREAMLOFT3D, CC BY 4.0";
    }
    std::printf("pointer: page %u hotTab %d cursor %d beam %d\n", page, v.hotTab, v.cursorOn ? 1 : 0, v.beamOn ? 1 : 0);
    return v;
}
} // namespace

// The cab and the screen from `eye` toward `target` (the van at the origin: world = van local).
bool cabView(const std::string& path, const std::string& models, Vec3 eye, Vec3 target, float focal) {
    Image image(1280, 960);
    fill(image, {.20f, .14f, .22f}, {.10f, .08f, .09f});
    van072::Frame f;
    const Vec3 back = normalized(eye - target), right = normalized(cross(Vec3{0, 1, 0}, back)), up = cross(back, right);
    std::vector<Vertex> cab;
    int loaded = 0;
    if (!models.empty()) {
        const std::string root = models + "/";
        // The built cab and the wheel (the body's 135k triangles are over the untextured draw's budget;
        // from the seat it is the windscreen frame only). Untextured: the base colours, darkened for night.
        mesh_model::Mesh inner, wheel;
        std::string err;
        if (mesh_model::loadFolder(root + "Vehicle/PunisherVan/Interior/", true, inner, err, mesh_model::vanTriangles) == mesh_model::loaded &&
            hero007::appendPlacedModel(cab, inner, f.at, f.forward(), {0, 1, 0}, eye))
            ++loaded;
        if (mesh_model::loadFolder(root + "Vehicle/PunisherVan/SteeringWheel/", true, wheel, err) == mesh_model::loaded) {
            Vec3 wu{}, toward{};
            van072::wheelAxes(0, wu, toward);
            if (hero007::appendPlacedModel(cab, wheel, f.world(van072::wheelHub), -van072::worldDir(f, toward), van072::worldDir(f, wu), eye))
                ++loaded;
        }
        std::printf("%s: cab models drawn: %d of 2\n", path.c_str(), loaded);
        for (auto& c : cab)
            c.color = c.color * .38f;
    }
    const Camera cam{focal, 640.f, 480.f};
    std::vector<float> depth(static_cast<size_t>(image.w) * image.h, std::numeric_limits<float>::infinity());
    if (!cab.empty())
        draw(image, toCamera(cab, eye, right, up, back), cam, 0, 0, 0, 0, image.w, image.h, &depth);
    vs::Screen s;
    s.next(); // STATUS
    vs::View v = vs::view(f, s, 8.9f, 16.f, .42f, true);
    v.flash = 0;
    std::vector<Vertex> w;
    bool good = vs::append(w, v, eye);
    // depth-tested against the cab, as the game draws it
    draw(image, toCamera(w, eye, right, up, back), cam, 0, 0, 0, 0, image.w, image.h, cab.empty() ? nullptr : &depth);
    return save(image, path) && good;
}
int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "spidy_van_screen_preview <folder> [<models folder>]\n");
        return 2;
    }
    const std::string dir = std::string(argv[1]) + "/";
    bool good = true;
    const float yaw = -0.79f; // heading about 045 NE
    good &= page(dir + "page_map.bmp", pageView(vs::map, 0, 10.4f, yaw, .55f));
    good &= page(dir + "page_status.bmp", pageView(vs::status, 0, 10.4f, yaw, .55f));
    for (uint32_t k = 0; k < vs::files; ++k)
        good &= page(dir + "page_cases_" + std::to_string(k + 1) + ".bmp", pageView(vs::cases, k, 10.4f, yaw, .55f));
    good &= page(dir + "page_locker.bmp", pageView(vs::locker, 0, 10.4f, yaw, .55f));
    // 0.0.74 test 1: MUSIC (a song playing; the folder empty) and HOME (standing; sat)
    {
        vs::View v = pageView(vs::music, 0, 10.4f, yaw, .55f);
        v.musicTracks = 14;
        v.musicTrack = 4;
        v.musicPlaying = true;
        v.musicSong = "Example Artist - Example Song";
        v.musicVolume = 70;
        v.musicTotal = 214;
        v.musicElapsed = 81;
        good &= page(dir + "page_music.bmp", v);
        vs::View e = pageView(vs::music, 0, 10.4f, yaw, .55f);
        e.musicTracks = 0;
        good &= page(dir + "page_music_empty.bmp", e);
        vs::View h = pageView(vs::home, 0, 10.4f, yaw, .55f);
        good &= page(dir + "page_home.bmp", h);
        h.offer.sat = true;
        good &= page(dir + "page_home_sat.bmp", h);
    }
    // 0.0.74 test 1 BATMOBILE: Batman's dash (the bat palette; MAP, STATUS, MUSIC, HOME)
    {
        const auto bat = [&](uint32_t pg) {
            vs::View v = pageView(pg, 0, 10.4f, yaw, .55f);
            v.offer.batman = true;
            v.offer.car = 0;
            v.carName = "1989";
            v.carCredit = "MODEL: BAT-MOBILE (1989) BY DREAMLOFT3D, CC BY 4.0";
            return v;
        };
        good &= page(dir + "bat_map.bmp", bat(vs::map));
        good &= page(dir + "bat_status.bmp", bat(vs::status));
        vs::View m = bat(vs::music);
        m.musicTracks = 14;
        m.musicTrack = 4;
        m.musicPlaying = true;
        m.musicSong = "Example Artist - Example Song";
        m.musicVolume = 70;
        m.musicTotal = 214;
        m.musicElapsed = 81;
        good &= page(dir + "bat_music.bmp", m);
        vs::View h = bat(vs::home);
        good &= page(dir + "bat_home.bmp", h);
        h.offer.car = 2;
        good &= page(dir + "bat_home_picked_forever.bmp", h);
        h.offer.car = 0;
        h.offer.sat = true;
        good &= page(dir + "bat_home_sat.bmp", h);
    }
    // 0.0.74 test 2 DASH POINTER: the ray on the MAP tab from the STATUS page (Batmobile and van), and a fingertip
    // over the LOCKER tab (van)
    good &= page(dir + "pointer_bat_ray_map.bmp", pointerView(vs::status, vs::map, true, false, yaw));
    good &= page(dir + "pointer_van_ray_map.bmp", pointerView(vs::status, vs::map, false, false, yaw));
    good &= page(dir + "pointer_van_finger_locker.bmp", pointerView(vs::map, vs::locker, false, true, yaw));
    // the driver's seat
    const std::string models = argc > 2 ? std::string(argv[2]) : std::string();
    // From the driver's eye (he sits left of it, so the flush face is seen a little from the side).
    good &= cabView(dir + "driver_seat.bmp", models, van072::seatEye + Vec3{.02f, 0, 0}, vs::mountAt + Vec3{-.16f, .02f, 0}, 760.f);
    // Straight on from the cab (centre, at the screen's height): flush in the dash face, square, no angle.
    good &= cabView(dir + "dash_front.bmp", models, vs::mountAt + Vec3{0, 0, .62f}, vs::mountAt, 900.f);
    std::printf("%s\n", good ? "van screen previews written" : "van screen previews: a page FAILED");
    return good ? 0 : 1;
}
