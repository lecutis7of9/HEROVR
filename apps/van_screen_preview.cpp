// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.73 test 3 VAN SCREEN: the dash screen's pages, drawn without a headset by the overlay's own
// vertices (van_screen::append, the code the game runs) - SIMULATED, NOT GAME CAPTURE. Into <folder>:
//   page_map.bmp page_status.bmp page_cases_N.bmp (N 1..7) page_locker.bmp  each page face-on
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
    // the driver's seat
    const std::string models = argc > 2 ? std::string(argv[2]) : std::string();
    // From the driver's eye (he sits left of it, so the flush face is seen a little from the side).
    good &= cabView(dir + "driver_seat.bmp", models, van072::seatEye + Vec3{.02f, 0, 0}, vs::mountAt + Vec3{-.16f, .02f, 0}, 760.f);
    // Straight on from the cab (centre, at the screen's height): flush in the dash face, square, no angle.
    good &= cabView(dir + "dash_front.bmp", models, vs::mountAt + Vec3{0, 0, .62f}, vs::mountAt, 900.f);
    std::printf("%s\n", good ? "van screen previews written" : "van screen previews: a page FAILED");
    return good ? 0 : 1;
}
