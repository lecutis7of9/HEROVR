// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.7 HERO PROFILES: draws the new looks without a headset - made-up hands,
// bodies and enemies over a dusk sky, through the overlay's own vertices (the same code the
// game runs). Writes into <folder>:
//   baton_before_after.bmp  the 0.0.62 baton (left) and BATON LOOK CLUB (right), close up
//   punisher_guns.bmp       pistol + readout, sawed-off, sawed-off broken open, spent cases
//   holsters.bmp            the body seen from behind: two hip pistols, the back sawed-off; and
//                           DAREDEVIL's back baton tubes (BATON HOLSTERS)
//   batman.bmp              a batarang in each hand, one thrown, smoke where it struck
//   batarang_styles.bmp     DEFAULT next to a STAND-IN TEST MESH through the model path (not an
//                           artist's model: those are drop-ins the player adds)
//   superman_wolverine.bmp  heat vision, cold breath, the claws
// With <Models folder> (the converted drop-ins, tools/models007/convert_models.py), also:
//   models_runtime.bmp      every converted model through the game's own model path
//   instances.bmp           exactly TWO batons (DAREDEVIL), TWO pistols + ONE sawed-off (PUNISHER),
//                           ONE batarang a hand (BATMAN), the converted models
//   baton_cable.bmp         BATON LOOK MODEL: no cord while held (before); the cable only on a throw (after)
//
//   spidy_hero007_preview <folder> [<Models folder>]
#include "spidy/hero007_visual.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/pixel_font.hpp"
#include "spidy/soft_shapes.hpp"
#include "preview_image.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace spidy;
using namespace spidy::preview;
using namespace spidy::hero007;

namespace {
constexpr int size = 900;
void dusk(Image& image, int x0, int w, int h) {
    for (int y = 0; y < h; ++y)
        for (int x = x0; x < x0 + w; ++x) {
            const float v = static_cast<float>(y) / static_cast<float>(h);
            float* o = image.at(x, y);
            const bool below = v > .70f;
            o[0] = decode(below ? .14f : .16f + .28f * v);
            o[1] = decode(below ? .14f : .15f + .16f * v);
            o[2] = decode(below ? .17f : .28f + .12f * v);
        }
}
std::vector<Vertex> look(const std::vector<Vertex>& world, Vec3 eye, Vec3 target) {
    const Vec3 back = normalized(eye - target);
    const Vec3 right = normalized(cross(Vec3{0, 1, 0}, back));
    const Vec3 up = cross(back, right);
    auto out = world;
    for (auto& v : out) {
        const Vec3 d = v.position - eye;
        v.position = {dot(d, right), dot(d, up), dot(d, back)};
    }
    return out;
}
void label(std::vector<Vertex>& out, const std::string& a, const std::string& b, Vec3 at, Vec3 eye, float k) {
    const Vec3 back = normalized(eye - at);
    const Vec3 right = normalized(cross(Vec3{0, 1, 0}, back));
    const Vec3 up = cross(back, right);
    PixelTextStyle s;
    s.height = .022f * k;
    s.top = s.bottom = Vec3{.55f, .85f, 1};
    appendPixelText(out, a, at - right * (pixelTextWidth(a, s) * .5f) + up * (.03f * k), right, up, s);
    s.height = .016f * k;
    s.top = s.bottom = Vec3{1, 1, 1};
    appendPixelText(out, b, at - right * (pixelTextWidth(b, s) * .5f), right, up, s);
}
// A dark stand-in body (feet at `f`), as the other previews draw enemies / the player.
void body(std::vector<Vertex>& w, Vec3 f, Vec3 eye, Vec3 colour) {
    soft::softBar(w, f + Vec3{0, .1f, 0}, f + Vec3{0, 1.45f, 0}, normalized(f + Vec3{0, 1, 0} - eye), .34f, colour, 1, .002f);
    soft::softBar(w, f + Vec3{0, 1.5f, 0}, f + Vec3{0, 1.78f, 0}, normalized(f + Vec3{0, 1.6f, 0} - eye), .17f, colour, 1, .002f);
}
void hand(std::vector<Vertex>& out, Vec3 at, Vec3 eye) {
    soft::softBar(out, at - Vec3{0, .045f, 0}, at + Vec3{0, .01f, 0}, normalized(at - eye), .02f, {.03f, .03f, .035f}, .9f, .002f);
}
struct Cell {
    std::vector<Vertex> world;
    Vec3 eye, target;
    float pixelAngle;
};
bool write(const std::string& path, std::vector<Cell>& cells) {
    Image image(size * static_cast<int>(cells.size()), size);
    for (size_t c = 0; c < cells.size(); ++c) {
        const int x0 = static_cast<int>(c) * size;
        dusk(image, x0, size, size);
        const Camera camera{1 / cells[c].pixelAngle, size * .5f, size * .5f};
        draw(image, look(cells[c].world, cells[c].eye, cells[c].target), camera, 0, 0, x0, 0, size, size);
        std::printf("  cell %zu: %zu vertices\n", c, cells[c].world.size());
    }
    return save(image, path);
}
// A batarang-shaped stand-in (an extruded scalloped outline, 6 mm thick, ~120 triangles).
mesh_model::Mesh testMesh() {
    mesh_model::Mesh m;
    std::vector<Vec3> ring;
    const float pts[][2] = {{0, .025f}, {.02f, .012f}, {.05f, .02f}, {.1f, .045f}, {.085f, .01f}, {.07f, -.005f}, {.05f, -.02f},
                            {.03f, -.012f}, {0, -.035f}};
    for (const auto& p : pts)
        ring.push_back({p[0], p[1], 0});
    for (int i = 7; i >= 1; --i)
        ring.push_back({-pts[i][0], pts[i][1], 0});
    const size_t n = ring.size();
    const Vec3 dark{.05f, .05f, .06f}, edge{.18f, .18f, .2f};
    const Vec3 lift{0, 0, .003f};
    for (size_t i = 0; i < n; ++i) {
        const Vec3 a = ring[i], b = ring[(i + 1) % n];
        for (const float side : {1.f, -1.f}) { // both faces as a fan from the centre
            m.corners.insert(m.corners.end(), {Vec3{0, 0, 0} + lift * side, a + lift * side, b + lift * side});
            m.colors.push_back(dark);
        }
        m.corners.insert(m.corners.end(), {a + lift, b + lift, b - lift}); // the rim
        m.colors.push_back(edge);
        m.corners.insert(m.corners.end(), {a + lift, b - lift, a - lift});
        m.colors.push_back(edge);
    }
    m.low = {-.1f, -.035f, -.003f};
    m.high = {.1f, .045f, .003f};
    m.sourceTriangles = m.triangles();
    return m;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: spidy_hero007_preview <folder>\n");
        return 2;
    }
    const std::string dir = std::string(argv[1]) + "/";
    bool ok = true;
    { // the baton, before and after
        std::vector<Cell> cells(2);
        for (int c = 0; c < 2; ++c) {
            Cell& k = cells[static_cast<size_t>(c)];
            k.eye = {0, 1.5f, 1.0f};
            k.target = {.12f, 1.45f, 0};
            k.pixelAngle = .0009f;
            Item it{c == 0 ? lookBaton062 : lookClub, {-.1f, 1.33f, 0}, normalized(Vec3{.75f, .55f, -.1f}), {0, 0, 1}};
            hand(k.world, it.at, k.eye);
            appendItem(k.world, it, k.eye, k.pixelAngle);
            label(k.world, c == 0 ? "BEFORE: 0.0.62 BATON" : "AFTER: BATON LOOK CLUB",
                  c == 0 ? "THE SHIPPED LOOK (STILL THE DEFAULT)" : "RIBBED GRIP, LACQUER SHAFT, STEEL CAPS (OFF BY DEFAULT)",
                  {.12f, 1.72f, 0}, k.eye, 1.0f);
        }
        std::printf("baton_before_after\n");
        ok &= write(dir + "baton_before_after.bmp", cells);
    }
    { // PUNISHER guns
        std::vector<Cell> cells(3);
        const Look looks3[3] = {lookPistol, lookSawedOff, lookSawedOffOpen};
        const char* titles[3] = {"PISTOL (15 ROUNDS)", "SAWED-OFF (2 BARRELS)", "SAWED-OFF BROKEN OPEN"};
        const char* subs[3] = {"STEEL SLIDE, POLYMER FRAME, READOUT ON TOP", "TWIN STEEL BARRELS, WOOD GRIP",
                               "REALISTIC RELOAD: SHELLS OUT, CLOSE TO LOAD"};
        const char* ammo[3] = {"12/15", "2/2", "0/2"};
        for (int c = 0; c < 3; ++c) {
            Cell& k = cells[static_cast<size_t>(c)];
            k.eye = {-.75f, 1.42f, -.12f};
            k.target = {0, 1.36f, -.12f};
            k.pixelAngle = .0009f;
            Item it{looks3[c], {0, 1.3f, 0}, normalized(Vec3{.12f, .03f, -1}), {0, 1, 0}};
            hand(k.world, it.at, k.eye);
            appendItem(k.world, it, k.eye, k.pixelAngle);
            appendAmmo(k.world, ammo[c], it, k.eye);
            if (c != 1)
                for (int s = 0; s < 3; ++s)
                    appendShell(k.world, Vec3{.02f, 1.42f + .05f * s, .02f + .06f * s}, {1, .3f, 0}, c == 2, .1f + .15f * s, k.eye,
                                k.pixelAngle);
            label(k.world, titles[c], subs[c], {0, 1.56f, -.12f}, k.eye, .85f);
        }
        std::printf("punisher_guns\n");
        ok &= write(dir + "punisher_guns.bmp", cells);
    }
    { // holsters on the body, seen from behind and to the side
        std::vector<Cell> cells(2);
        for (int c = 0; c < 2; ++c) {
            Cell& k = cells[static_cast<size_t>(c)];
            k.eye = {1.1f, 1.55f, 1.35f};
            k.target = {0, 1.1f, 0};
            k.pixelAngle = .0016f;
            body(k.world, {0, 0, 0}, k.eye, c == 0 ? Vec3{.06f, .06f, .07f} : Vec3{.30f, .04f, .04f});
            if (c == 0) {
                appendItem(k.world, Item{lookPistol, {.24f, .98f, .02f}, {0, -1, 0}, {0, 0, -1}, 0, 1}, k.eye, k.pixelAngle);
                appendItem(k.world, Item{lookPistol, {-.24f, .98f, .02f}, {0, -1, 0}, {0, 0, -1}, 0, 1}, k.eye, k.pixelAngle);
                appendItem(k.world, Item{lookSawedOff, {.12f, 1.48f, .2f}, normalized(Vec3{-.45f, -1, 0}), {0, 0, 1}, 0, 2}, k.eye,
                           k.pixelAngle);
                label(k.world, "PUNISHER HOLSTERS", "A PISTOL ON EACH HIP, THE SAWED-OFF ON THE BACK", {0, 2.02f, 0}, k.eye, 1.9f);
            } else {
                appendItem(k.world, Item{lookBaton062, {.1f, 1.45f, .2f}, normalized(Vec3{-.35f, -1, 0}), {0, 0, 1}, 0, 3}, k.eye,
                           k.pixelAngle);
                appendItem(k.world, Item{lookBaton062, {-.1f, 1.45f, .2f}, normalized(Vec3{.35f, -1, 0}), {0, 0, 1}, 0, 3}, k.eye,
                           k.pixelAngle);
                label(k.world, "DAREDEVIL BATON HOLSTERS", "TWO BATONS IN BACK TUBES (OFF BY DEFAULT)", {0, 2.02f, 0}, k.eye, 1.9f);
            }
        }
        std::printf("holsters\n");
        ok &= write(dir + "holsters.bmp", cells);
    }
    { // BATMAN
        std::vector<Cell> cells(1);
        Cell& k = cells[0];
        const Vec3 head{0, 1.6f, 0};
        k.eye = head;
        k.target = head + Vec3{0, -.45f, -1};
        k.pixelAngle = .0016f;
        Draw d;
        d.any = true;
        d.held[0] = Item{lookBatarang, {-.2f, 1.25f, -.38f}, normalized(Vec3{.1f, .2f, -1}), {0, 1, 0}};
        d.held[1] = Item{lookNone};
        d.flying[1] = Item{lookBatarang, {.35f, 1.35f, -2.2f}, {0, 0, -1}, {0, 1, 0}, .8f};
        d.smokeAt[0] = {.9f, 1.2f, -5.5f};
        d.smokeAge[0] = .35f;
        d.smokeAt[1] = {1.0f, 1.3f, -5.4f};
        d.smokeAge[1] = .9f;
        body(k.world, {1.0f, 0, -6}, k.eye, {.05f, .05f, .07f});
        hand(k.world, d.held[0].at, k.eye);
        hand(k.world, {.22f, 1.28f, -.35f}, k.eye);
        appendHero007(k.world, d, {}, k.eye, k.pixelAngle);
        label(k.world, "BATMAN", "A BATARANG A HAND; ONE THROWN; SMOKE WHERE IT STRUCK", head + Vec3{0, -.05f, -1.5f},
              k.eye, 2.2f);
        std::printf("batman\n");
        ok &= write(dir + "batman.bmp", cells);
    }
    { // BATARANG STYLE: DEFAULT vs the model path with a stand-in mesh
        std::vector<Cell> cells(2);
        static const mesh_model::Mesh stand = testMesh();
        for (int c = 0; c < 2; ++c) {
            Cell& k = cells[static_cast<size_t>(c)];
            k.eye = {0, 1.5f, .5f};
            k.target = {0, 1.42f, 0};
            k.pixelAngle = .0006f;
            Draw d;
            d.any = true;
            d.held[0] = Item{lookBatarang, {0, 1.36f, 0}, normalized(Vec3{0, .25f, -1}), {0, 1, 0}};
            d.batarangModel = c == 1 ? &stand : nullptr;
            appendHero007(k.world, d, {}, k.eye, k.pixelAngle);
            label(k.world, c == 0 ? "BATARANG STYLE: DEFAULT" : "MODEL PATH: STAND-IN TEST MESH",
                  c == 0 ? "THE BUILT-IN BATARANG" : "NOT AN ARTIST MODEL: EIRA / CLOONEY / JOKER ARE DROP-INS", {0, 1.53f, 0}, k.eye,
                  .5f);
        }
        std::printf("batarang_styles\n");
        ok &= write(dir + "batarang_styles.bmp", cells);
    }
    { // SUPERMAN heat / freeze, WOLVERINE claws
        std::vector<Cell> cells(3);
        const Vec3 head{0, 1.6f, 0};
        for (int c = 0; c < 3; ++c) {
            Cell& k = cells[static_cast<size_t>(c)];
            k.pixelAngle = .0016f;
            if (c < 2) {
                k.eye = head + Vec3{-1.6f, .5f, 2.2f};
                k.target = head + Vec3{.3f, -.4f, -4};
                body(k.world, {0, 0, 0}, k.eye, {.08f, .12f, .45f});
                body(k.world, {.4f, 0, -8}, k.eye, {.05f, .05f, .07f});
                Draw d;
                d.any = true;
                d.beamFade = 1;
                d.beamFreeze = c == 1;
                d.beamFrom[0] = head + Vec3{-.032f, .02f, -.1f};
                d.beamFrom[1] = head + Vec3{.032f, .02f, -.1f};
                d.beamTo = Vec3{.4f, 1.3f, -8};
                if (c == 1)
                    d.beamFrom[0] = d.beamFrom[1] = head + Vec3{0, -.08f, -.12f};
                appendHero007(k.world, d, {}, k.eye, k.pixelAngle);
                label(k.world, c == 0 ? "SUPERMAN: HEAT VISION" : "SUPERMAN: COLD BREATH",
                      c == 0 ? "BOTH TRIGGERS: BEAMS ALONG YOUR GAZE, 3 S THEN COOL" : "A HAND AT THE MOUTH: FROST (DAMAGE + LOOK ONLY)",
                      head + Vec3{.35f, .55f, -1.f}, k.eye, 4.5f);
            } else {
                k.eye = {-.7f, 1.5f, .0f};
                k.target = {.05f, 1.35f, -.3f};
                k.pixelAngle = .0011f;
                Item it{lookClaws, {.05f, 1.3f, -.15f}, normalized(Vec3{.2f, .1f, -1}), {0, 1, 0}};
                hand(k.world, it.at, k.eye);
                appendItem(k.world, it, k.eye, k.pixelAngle);
                label(k.world, "WOLVERINE: CLAWS", "TRIGGER: THREE 30 CM CLAWS, SWING TO CUT; HEALING", {.05f, 1.62f, -.3f}, k.eye, 1.0f);
            }
        }
        std::printf("superman_wolverine\n");
        ok &= write(dir + "superman_wolverine.bmp", cells);
    }
    if (argc >= 3) {
        std::string root = argv[2];
        if (!root.empty() && root.back() != '/')
            root += '/';
        mesh_model::Library& lib = mesh_model::library();
        for (uint32_t sl = 1; sl < mesh_model::slots; ++sl) {
            const auto st = lib.load(sl, root);
            const auto* m = lib.model(sl);
            std::printf("  model %-18s %s, %u triangles%s %s\n", mesh_model::slotFolders[sl], mesh_model::statusNames[st < 5 ? static_cast<int>(st) : 0],
                        m ? m->triangles() : 0u, m && m->decimated ? " (decimated at load)" : "", lib.error[sl].c_str());
        }
        auto tris = [&](uint32_t sl) { return lib.model(sl) ? std::to_string(lib.model(sl)->triangles()) + " TRIANGLES" : std::string("NOT LOADED"); };
        { // every converted model through the game's model path
            std::vector<Cell> cells(4);
            for (int c = 0; c < 4; ++c) {
                Cell& k = cells[static_cast<size_t>(c)];
                k.eye = {0, 1.5f, .9f};
                k.target = {0, 1.42f, 0};
                k.pixelAngle = .0008f;
            }
            const char* names[3] = {"EIRA", "CLOONEY", "JOKER"};
            for (int b = 0; b < 3; ++b) { // the three batarang styles, side by side in cell 0
                const auto* m = lib.model(static_cast<uint32_t>(mesh_model::styleEira + b));
                Cell& k = cells[0];
                Item it{lookBatarang, {-.24f + .24f * static_cast<float>(b), 1.4f, 0}, normalized(Vec3{0, .3f, -1}), {0, 1, 0}};
                if (!m || !appendBatarangModel(k.world, it, *m, k.eye))
                    appendItem(k.world, it, k.eye, k.pixelAngle);
                label(k.world, names[b], tris(static_cast<uint32_t>(mesh_model::styleEira + b)), it.at + Vec3{0, -.1f, 0}, k.eye, .6f);
            }
            label(cells[0].world, "BATARANG STYLE", "EIRA, CLOONEY, JOKER (JOKER AS DOWNLOADED)", {0, 1.62f, 0}, cells[0].eye, .8f);
            { // pistol
                Cell& k = cells[1];
                k.eye = {-.6f, 1.44f, -.1f};
                k.target = {0, 1.38f, -.1f};
                Item it{lookPistol, {0, 1.33f, 0}, normalized(Vec3{.12f, .03f, -1}), {0, 1, 0}};
                hand(k.world, it.at, k.eye);
                if (!lib.model(mesh_model::slotPistol) || !appendModel(k.world, it, *lib.model(mesh_model::slotPistol), k.eye))
                    appendItem(k.world, it, k.eye, k.pixelAngle);
                label(k.world, "PISTOL (TORI106)", "GUN MODELS: " + tris(mesh_model::slotPistol), {0, 1.47f, -.1f}, k.eye, .6f);
            }
            { // sawed-off closed and broken open
                Cell& k = cells[2];
                k.eye = {-1.0f, 1.42f, -.2f};
                k.target = {0, 1.33f, -.2f};
                k.pixelAngle = .0011f;
                const auto* fr = lib.model(mesh_model::slotSawedOffFrame);
                const auto* ba = lib.model(mesh_model::slotSawedOffBarrels);
                for (int o = 0; o < 2; ++o) {
                    Item it{o ? lookSawedOffOpen : lookSawedOff, {0, 1.42f - .2f * static_cast<float>(o), 0}, normalized(Vec3{.1f, .02f, -1}), {0, 1, 0}};
                    if (!fr || !ba || !appendSawedOffModel(k.world, it, *fr, *ba, o == 1, k.eye))
                        appendItem(k.world, it, k.eye, k.pixelAngle);
                }
                label(k.world, "SAWED-OFF (LILIUMLETIFER)", "SPLIT AT THE HINGE: CLOSED / BROKEN OPEN", {0, 1.6f, -.2f}, k.eye, 1.1f);
            }
            { // the two batons
                Cell& k = cells[3];
                k.eye = {0, 1.45f, 1.1f};
                k.target = {0, 1.45f, 0};
                k.pixelAngle = .0011f;
                for (uint32_t sd = 0; sd < 2; ++sd) {
                    const Vec3 grip{sd ? .12f : -.12f, 1.15f, 0};
                    const Vec3 tip = grip + normalized(Vec3{sd ? .25f : -.25f, 1, 0}) * .57f;
                    const auto* m = lib.model(mesh_model::slotBatonLeft + sd);
                    if (!m || !appendBatonModel(k.world, grip, tip, *m, k.eye))
                        appendItem(k.world, Item{lookClub, grip, normalized(tip - grip), {0, 0, 1}}, k.eye, k.pixelAngle);
                }
                label(k.world, "BATONS (ANTHONY YANEZ)", "ROPE REMOVED, SPLIT: LEFT " + tris(mesh_model::slotBatonLeft) + ", RIGHT " +
                      tris(mesh_model::slotBatonRight), {0, 1.78f, 0}, k.eye, .8f);
            }
            std::printf("models_runtime\n");
            ok &= write(dir + "models_runtime.bmp", cells);
        }
        { // exactly two a hero
            std::vector<Cell> cells(3);
            for (int c = 0; c < 3; ++c) {
                Cell& k = cells[static_cast<size_t>(c)];
                k.eye = {1.1f, 1.55f, 1.35f};
                k.target = {0, 1.1f, 0};
                k.pixelAngle = .0016f;
                Draw d;
                d.any = true;
                d.pistolModel = lib.model(mesh_model::slotPistol);
                d.sawedFrame = lib.model(mesh_model::slotSawedOffFrame);
                d.sawedBarrels = lib.model(mesh_model::slotSawedOffBarrels);
                d.batonModel[0] = lib.model(mesh_model::slotBatonLeft);
                d.batonModel[1] = lib.model(mesh_model::slotBatonRight);
                d.batarangModel = lib.model(mesh_model::styleEira);
                const char* t = "";
                const char* sub = "";
                if (c == 0) {
                    body(k.world, {0, 0, 0}, k.eye, {.30f, .04f, .04f});
                    d.worn[0] = Item{lookBatonModel, {.1f, 1.45f, .2f}, normalized(Vec3{-.35f, -1, 0}), {0, 0, 1}, 0, 3, 0};
                    d.worn[1] = Item{lookBatonModel, {-.1f, 1.45f, .2f}, normalized(Vec3{.35f, -1, 0}), {0, 0, 1}, 0, 3, 1};
                    t = "DAREDEVIL: EXACTLY 2 BATONS";
                    sub = "BOTH IN THE BACK TUBES (BATON LOOK MODEL)";
                } else if (c == 1) {
                    body(k.world, {0, 0, 0}, k.eye, {.06f, .06f, .07f});
                    d.worn[0] = Item{lookPistol, {.24f, .98f, .02f}, {0, -1, 0}, {0, 0, -1}, 0, 1};
                    d.worn[1] = Item{lookPistol, {-.24f, .98f, .02f}, {0, -1, 0}, {0, 0, -1}, 0, 1};
                    d.worn[2] = Item{lookSawedOff, {.12f, 1.48f, .2f}, normalized(Vec3{-.45f, -1, 0}), {0, 0, 1}, 0, 2};
                    t = "PUNISHER: 2 PISTOLS + 1 SAWED-OFF";
                    sub = "A PISTOL ON EACH HIP, THE SAWED-OFF ON THE BACK (GUN MODELS)";
                } else {
                    k.eye = {0, 1.6f, 0};
                    k.target = {0, 1.05f, -1};
                    k.pixelAngle = .0014f;
                    d.held[0] = Item{lookBatarang, {-.2f, 1.25f, -.38f}, normalized(Vec3{.1f, .2f, -1}), {0, 1, 0}};
                    d.held[1] = Item{lookBatarang, {.2f, 1.25f, -.38f}, normalized(Vec3{-.1f, .2f, -1}), {0, 1, 0}};
                    hand(k.world, d.held[0].at, k.eye);
                    hand(k.world, d.held[1].at, k.eye);
                    t = "BATMAN: 1 BATARANG A HAND";
                    sub = "BATARANG STYLE EIRA";
                }
                appendHero007(k.world, d, {}, k.eye, k.pixelAngle);
                label(k.world, t, sub, c == 2 ? Vec3{0, 1.45f, -1.2f} : Vec3{0, 2.02f, 0}, k.eye, c == 2 ? 1.6f : 1.9f);
            }
            std::printf("instances\n");
            ok &= write(dir + "instances.bmp", cells);
        }
        { // the cable: none while held, only while thrown
            std::vector<Cell> cells(2);
            const Vec3 head{0, 1.6f, 0};
            for (int c = 0; c < 2; ++c) {
                Cell& k = cells[static_cast<size_t>(c)];
                k.eye = head + Vec3{-.9f, .3f, .9f};
                k.target = head + Vec3{.2f, -.3f, -1.4f};
                k.pixelAngle = .0014f;
                Draw d;
                d.any = true;
                d.batonModel[0] = lib.model(mesh_model::slotBatonLeft);
                d.batonModel[1] = lib.model(mesh_model::slotBatonRight);
                const Vec3 handL{-.22f, 1.3f, -.35f}, handR{.22f, 1.3f, -.35f};
                hand(k.world, handL, k.eye);
                hand(k.world, handR, k.eye);
                d.held[0] = Item{lookBatonModel, handL, normalized(Vec3{.1f, .35f, -1}), {0, 1, 0}, 0, 0, 0};
                if (c == 0) {
                    d.held[1] = Item{lookBatonModel, handR, normalized(Vec3{-.1f, .35f, -1}), {0, 1, 0}, 0, 0, 1};
                } else {
                    // the right baton thrown: drawn where it flies, the cable from the hand to it
                    const Vec3 at{.6f, 1.4f, -2.6f};
                    const auto* m = d.batonModel[1];
                    if (!m || !appendBatonModel(k.world, at, at + normalized(Vec3{.3f, .2f, -1}) * .57f, *m, k.eye))
                        appendItem(k.world, Item{lookClub, at, normalized(Vec3{.3f, .2f, -1}), {0, 1, 0}}, k.eye, k.pixelAngle);
                    d.cable[1] = true;
                    d.cableFrom[1] = handR;
                    d.cableTo[1] = at;
                }
                appendHero007(k.world, d, {}, k.eye, k.pixelAngle);
                label(k.world, c == 0 ? "BEFORE: HELD, NO CORD" : "AFTER: THROWN, CABLE OUT",
                      c == 0 ? "NO PERMANENT CORD BETWEEN THE BATONS" : "THE CABLE ONLY WHILE A BATON IS THROWN OR BOUNCING",
                      head + Vec3{.2f, .25f, -1.6f}, k.eye, 1.8f);
            }
            std::printf("baton_cable\n");
            ok &= write(dir + "baton_cable.bmp", cells);
        }
    }
    return ok ? 0 : 1;
}
