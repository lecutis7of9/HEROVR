// HERO VR 0.0.76 SM1 HERO PROPS: the off-headset preview (a developer's tool, Linux; never shipped). Made by Agent Mulder.
// Loads the heroes' real models (the SM2 build's, THIRD-PARTY-MODELS.md) from a HERO VR Models folder, puts them where
// the SM1 layer puts them (sm1_props.hpp: scene(), the throws, the game-to-headset map) and draws both eyes the way
// sm1_props_gpu.cpp's shader does (eyeViewProjection, modelMatrices, the same lighting, steel and cutout), on the CPU,
// over a stand-in "game picture" (the props have no depth of the game's: they always draw over it).
// Usage: sm1_props_preview <...\Models\ (with the slash)> <out folder>
// PNG/JPEG textures are decoded by python3 + Pillow (the layer uses WIC).
#include "spidy/sm1_props.hpp"
#include "spidy/hero_modes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace spidy;
namespace props = spidy::sm1::props;

namespace {
bool pilDecode(const uint8_t* data, size_t size, mesh_model::Picture& out) {
    const std::string in = "/tmp/sm1pp_in.bin", raw = "/tmp/sm1pp_out.bin";
    {
        std::ofstream f(in, std::ios::binary);
        f.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    }
    const std::string cmd = "python3 -c \"import sys;from PIL import Image;im=Image.open('" + in +
                            "').convert('RGBA');open('" + raw +
                            "','wb').write(('%d %d\\n'%im.size).encode()+im.tobytes())\" 2>/dev/null";
    if (std::system(cmd.c_str()) != 0)
        return false;
    std::ifstream f(raw, std::ios::binary);
    uint32_t w = 0, h = 0;
    if (!(f >> w >> h) || !w || !h || w > 16384 || h > 16384)
        return false;
    f.get();
    out.width = w, out.height = h;
    out.rgba.resize(size_t{w} * h * 4);
    f.read(reinterpret_cast<char*>(out.rgba.data()), static_cast<std::streamsize>(out.rgba.size()));
    return f.gcount() == static_cast<std::streamsize>(out.rgba.size());
}

struct Loaded {
    std::vector<props::GpuVertex> verts[props::modelCount];
    props::Bounds bounds[props::modelCount];
    mesh_model::TextureArray tex;
};
bool loadHero(const std::string& root, uint32_t hero, Loaded& out) {
    auto lib = std::make_unique<mesh_model::Library>();
    for (props::Model m : props::modelsFor(hero)) {
        const uint32_t slot = props::slotOf(m);
        if (lib->load(slot, root) != mesh_model::loaded)
            std::printf("  %s: %s\n", props::modelNames[m], lib->error[slot].c_str());
    }
    out.tex = mesh_model::prepareTextures(*lib, &pilDecode, 256, 1);
    bool any = false;
    for (props::Model m : props::modelsFor(hero))
        if (const mesh_model::Mesh* mesh = lib->model(props::slotOf(m)); mesh && props::pack(*mesh, out.verts[m])) {
            out.bounds[m] = props::boundsOf(m, *mesh);
            std::printf("  %s: %zu triangles, %zu texture layers\n", props::modelNames[m], out.verts[m].size() / 3,
                        out.tex.layers.size());
            any = true;
        }
    return any;
}

struct Image {
    int w, h;
    std::vector<float> rgb, depth;
    Image(int w_, int h_) : w(w_), h(h_), rgb(size_t(w_) * h_ * 3), depth(size_t(w_) * h_, 1.f) {}
};
// The stand-in game picture: a city-ish gradient with a grid (the props draw over it, no occlusion).
void background(Image& im) {
    for (int y = 0; y < im.h; ++y)
        for (int x = 0; x < im.w; ++x) {
            const float t = float(y) / float(im.h);
            const bool grid = (x / 32 + y / 32) % 2 == 0;
            float* p = &im.rgb[(size_t(y) * im.w + x) * 3];
            p[0] = .30f + .25f * t + (grid ? .03f : 0);
            p[1] = .38f + .18f * t + (grid ? .03f : 0);
            p[2] = .52f - .10f * t + (grid ? .03f : 0);
        }
}
float srgbToLinear(uint8_t c) {
    const float v = c / 255.f;
    return v <= .04045f ? v / 12.92f : std::pow((v + .055f) / 1.055f, 2.4f);
}
float toSrgb(float c) {
    c = std::clamp(c, 0.f, 1.f);
    return c <= .0031308f ? c * 12.92f : 1.055f * std::pow(c, 1 / 2.4f) - .055f;
}
struct Shaded {
    Vec3 n, c, m;
    float u, v, layer;
};
// sm1_props_gpu.cpp's ps(), on the CPU (alpha < 0: discarded)
bool shade(const Shaded& i, const mesh_model::TextureArray& tex, bool steel, float reach, Vec3& out) {
    Vec3 n = i.n;
    const float len = length(n);
    n = len > 1e-6f ? n / len : Vec3{0, 1, 0};
    const Vec3 light = normalized(Vec3{.35f, .85f, .4f});
    const float lit = .32f + .68f * std::abs(dot(n, light));
    if (steel) {
        const float t = std::clamp(-i.m.z / std::max(reach, .05f), 0.f, 1.f);
        const float shine = std::pow(std::abs(dot(n, normalized(Vec3{.2f, .9f, .3f}))), 24.f);
        const Vec3 a{.50f, .52f, .56f}, b{.82f, .84f, .88f};
        out = (a + (b - a) * t) * lit + Vec3{shine, shine, shine} * .55f;
        return true;
    }
    Vec3 c = i.c;
    if (i.layer >= 0) {
        float layer = i.layer;
        const bool cut = layer >= 1023.5f;
        if (cut)
            layer -= 1024;
        const int L = int(layer + .5f);
        if (L >= 0 && L < int(tex.layers.size())) {
            const int s = int(tex.size);
            float u = i.u - std::floor(i.u), v = i.v - std::floor(i.v);
            const int x = std::clamp(int(u * s), 0, s - 1), y = std::clamp(int(v * s), 0, s - 1);
            const uint8_t* px = &tex.layers[size_t(L)][(size_t(y) * s + x) * 4];
            if (cut && px[3] < 128)
                return false;
            c = {c.x * srgbToLinear(px[0]), c.y * srgbToLinear(px[1]), c.z * srgbToLinear(px[2])};
        }
    }
    out = c * lit;
    return true;
}
Vec3 mulRows(const float* r, int stride, Vec3 v, bool affine) {
    return {r[0] * v.x + r[1] * v.y + r[2] * v.z + (affine ? r[3] : 0),
            r[stride] * v.x + r[stride + 1] * v.y + r[stride + 2] * v.z + (affine ? r[stride + 3] : 0),
            r[2 * stride] * v.x + r[2 * stride + 1] * v.y + r[2 * stride + 2] * v.z + (affine ? r[2 * stride + 3] : 0)};
}
// vs() + rasterizer (D3D: clip, perspective-correct, depth LESS, no culling) + ps()
struct Drawn {
    int pixels = 0, x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
};
Drawn draw(Image& im, const Mat4& vp, const props::Item& item, const Loaded& L) {
    Drawn dr;
    float model[12], normal[9];
    if (!props::modelMatrices(item.place, model, normal))
        return dr;
    const auto& vs = L.verts[item.model];
    struct Out {
        float x, y, z, w;
        Shaded s;
    };
    for (size_t t = 0; t + 2 < vs.size(); t += 3) {
        Out o[3];
        bool behind = false;
        for (int k = 0; k < 3; ++k) {
            const auto& v = vs[t + k];
            const Vec3 p{v.position[0], v.position[1], v.position[2]};
            const Vec3 w = mulRows(model, 4, p, true);
            const float c[4] = {vp[0] * w.x + vp[1] * w.y + vp[2] * w.z + vp[3], vp[4] * w.x + vp[5] * w.y + vp[6] * w.z + vp[7],
                                vp[8] * w.x + vp[9] * w.y + vp[10] * w.z + vp[11],
                                vp[12] * w.x + vp[13] * w.y + vp[14] * w.z + vp[15]};
            if (!(c[3] > 1e-4f) || c[2] < 0) {
                behind = true;
                break;
            }
            o[k].w = c[3];
            o[k].x = (c[0] / c[3] * .5f + .5f) * im.w;
            o[k].y = (.5f - c[1] / c[3] * .5f) * im.h;
            o[k].z = c[2] / c[3];
            o[k].s.n = mulRows(normal, 3, {v.normal[0], v.normal[1], v.normal[2]}, false);
            o[k].s.c = {v.colour[0], v.colour[1], v.colour[2]};
            o[k].s.m = p;
            o[k].s.u = v.uv[0], o[k].s.v = v.uv[1], o[k].s.layer = v.layer;
        }
        if (behind)
            continue;
        const float area = (o[1].x - o[0].x) * (o[2].y - o[0].y) - (o[2].x - o[0].x) * (o[1].y - o[0].y);
        if (std::abs(area) < 1e-9f)
            continue;
        const int x0 = std::max(0, int(std::floor(std::min({o[0].x, o[1].x, o[2].x}))));
        const int x1 = std::min(im.w - 1, int(std::ceil(std::max({o[0].x, o[1].x, o[2].x}))));
        const int y0 = std::max(0, int(std::floor(std::min({o[0].y, o[1].y, o[2].y}))));
        const int y1 = std::min(im.h - 1, int(std::ceil(std::max({o[0].y, o[1].y, o[2].y}))));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const float px = x + .5f, py = y + .5f;
                float b[3];
                b[0] = ((o[1].x - px) * (o[2].y - py) - (o[2].x - px) * (o[1].y - py)) / area;
                b[1] = ((o[2].x - px) * (o[0].y - py) - (o[0].x - px) * (o[2].y - py)) / area;
                b[2] = 1 - b[0] - b[1];
                if (b[0] < 0 || b[1] < 0 || b[2] < 0)
                    continue;
                const float z = b[0] * o[0].z + b[1] * o[1].z + b[2] * o[2].z;
                float& d = im.depth[size_t(y) * im.w + x];
                if (!(z < d))
                    continue;
                float q[3], sum = 0;
                for (int k = 0; k < 3; ++k)
                    sum += q[k] = b[k] / o[k].w;
                for (float& k : q)
                    k /= sum;
                Shaded s;
                s.n = o[0].s.n * q[0] + o[1].s.n * q[1] + o[2].s.n * q[2];
                s.c = o[0].s.c * q[0] + o[1].s.c * q[1] + o[2].s.c * q[2];
                s.m = o[0].s.m * q[0] + o[1].s.m * q[1] + o[2].s.m * q[2];
                s.u = o[0].s.u * q[0] + o[1].s.u * q[1] + o[2].s.u * q[2];
                s.v = o[0].s.v * q[0] + o[1].s.v * q[1] + o[2].s.v * q[2];
                s.layer = o[0].s.layer;
                Vec3 c;
                if (!shade(s, L.tex, item.steel, item.reach, c))
                    continue;
                d = z;
                float* p = &im.rgb[(size_t(y) * im.w + x) * 3];
                p[0] = c.x, p[1] = c.y, p[2] = c.z;
                ++dr.pixels;
                dr.x0 = std::min(dr.x0, x), dr.y0 = std::min(dr.y0, y), dr.x1 = std::max(dr.x1, x), dr.y1 = std::max(dr.y1, y);
            }
    }
    return dr;
}
void writePpm(const std::string& path, const Image& l, const Image& r) {
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << l.w + r.w + 8 << " " << l.h << "\n255\n";
    for (int y = 0; y < l.h; ++y) {
        for (int x = 0; x < l.w + r.w + 8; ++x) {
            unsigned char c[3] = {0, 0, 0};
            const Image* im = x < l.w ? &l : x >= l.w + 8 ? &r : nullptr;
            if (im) {
                const int xx = x < l.w ? x : x - l.w - 8;
                const float* p = &im->rgb[(size_t(y) * im->w + xx) * 3];
                for (int k = 0; k < 3; ++k)
                    c[k] = static_cast<unsigned char>(toSrgb(p[k]) * 255.f + .5f);
            }
            f.write(reinterpret_cast<const char*>(c), 3);
        }
    }
}

// A standing player in his projection space (stage, Y up, -Z ahead), arms out in front.
Pose head() { return {{0, 1.62f, 0}, Quat::around({1, 0, 0}, -.12f)}; }
props::Hands hands() {
    props::Hands h;
    // OpenXR grip: -Z along the controller's front, +Y up from the grip; tilted a little down and in
    h.grip[0] = {{-.20f, 1.30f, -.38f}, Quat::around({0, 1, 0}, .15f) * Quat::around({1, 0, 0}, -.35f)};
    h.grip[1] = {{.20f, 1.30f, -.38f}, Quat::around({0, 1, 0}, -.15f) * Quat::around({1, 0, 0}, -.35f)};
    h.tracked[0] = h.tracked[1] = true;
    h.kind = ControllerKind::touch;
    return h;
}
void render(const std::string& out, const char* name, uint32_t hero, const Loaded& L, const props::Flying& flying) {
    const auto items = props::scene(hero, hands(), L.bounds, flying);
    Image eyes[2]{Image(560, 560), Image(560, 560)};
    const Pose h = head();
    for (int e = 0; e < 2; ++e) {
        background(eyes[e]);
        Pose eye = h;
        eye.position = h.position + h.orientation.rotate({e ? .032f : -.032f, 0, 0});
        // Quest-3-like fovs (radians), mirrored per eye
        const float inner = .79f, outer = .96f;
        const Mat4 vp = props::eyeViewProjection(eye, e ? -inner : -outer, e ? outer : inner, .87f, -.92f);
        for (const auto& it : items) {
            const Drawn d = draw(eyes[e], vp, it, L);
            std::printf("  %s eye, %s: %d pixels in x %d..%d y %d..%d\n", e ? "right" : "left", props::modelNames[it.model],
                        d.pixels, d.x0, d.x1, d.y0, d.y1);
        }
    }
    const std::string ppm = out + "/" + name + ".ppm";
    writePpm(ppm, eyes[0], eyes[1]);
    std::printf("%s: %zu props drawn -> %s\n", name, items.size(), ppm.c_str());
}
// The layer's throw: launched in the GAME's world (its own axes and scale), stepped with sm1_props' tuning, and
// mapped into the headset's space with HeadMap exactly as propsFrame() does. The game head here is turned and moved
// away from the headset's on purpose, so a wrong map shows.
props::Flying throwAt(props::Model m, const props::Tuning& t, float seconds, bool aimed) {
    const Pose xh = head();
    const Vec3 gameHead{105.f, 12.f, -40.f};
    const Quat turn = Quat::around({0, 1, 0}, .9f);            // the game's yaw vs the stage's
    const Vec3 xrAhead = xh.orientation.rotate({0, 0, -1}), xrRight = xh.orientation.rotate({1, 0, 0}),
               xrUp = xh.orientation.rotate({0, 1, 0});
    // the game head's rows: right, down, forward (game: forward is where the eyes look)
    const Vec3 right = turn.rotate(xrRight), down = turn.rotate(xrUp * -1.f), forward = turn.rotate(xrAhead);
    const props::HeadMap map = props::headMap(gameHead, right, down, forward, xh);
    const Vec3 handGame = props::toGame(map, hands().grip[1].position);
    const Vec3 aimGame = normalized(forward + down * .05f);
    const Vec3 enemy = handGame + aimGame * 9.f + right * .8f;
    props::Flight f;
    props::launch(f, handGame, aimed ? normalized(enemy - handGame) : aimGame, aimed ? &enemy : nullptr, t);
    for (float s = 0; s < seconds && f.phase != props::Flight::held; s += 1.f / 90)
        props::step(f, t, handGame, aimed ? &enemy : nullptr, 1.f / 90);
    props::Flying fl;
    if (f.phase == props::Flight::held)
        return fl;
    fl.model = m;
    fl.middle = props::toXr(map, f.pos);
    fl.way = normalized(props::dirToXr(map, f.vel));
    fl.spin = m == props::hammer ? f.travelled * props::hammerSpinPerMetre : m == props::shield ? f.age * 14.f : f.age * 22.f; // as stepThrow
    std::printf("  %s after %.2f s: %s, %.1f m from the hand (headset space %.2f %.2f %.2f)\n", props::modelNames[m],
                double(seconds), f.phase == props::Flight::out ? "going out" : "coming back",
                double(length(f.pos - handGame)), double(fl.middle.x), double(fl.middle.y), double(fl.middle.z));
    return fl;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <Models folder with a trailing slash> <out folder>\n", argv[0]);
        return 2;
    }
    const std::string root = argv[1], out = argv[2];
    struct Hero {
        uint32_t id;
        const char* name;
    };
    int drawn = 0;
    for (const Hero h : {Hero{hero_modes::thor, "thor"}, Hero{hero_modes::captainAmerica, "cap"},
                         Hero{hero_modes::wolverine, "wolverine"}, Hero{hero_modes::daredevil, "daredevil"},
                         Hero{hero_modes::punisher, "punisher"}, Hero{hero_modes::batman, "batman"}}) {
        std::printf("%s:\n", h.name);
        Loaded L;
        if (!loadHero(root, h.id, L)) {
            std::printf("  no models\n");
            continue;
        }
        const std::string n = h.name;
        if (h.id == hero_modes::thor) {
            render(out, (n + "_held").c_str(), h.id, L, {});
            render(out, (n + "_thrown_out").c_str(), h.id, L, throwAt(props::hammer, props::hammerThrow, .30f, true));
            render(out, (n + "_coming_back").c_str(), h.id, L, throwAt(props::hammer, props::hammerThrow, .62f, true));
        } else if (h.id == hero_modes::captainAmerica) {
            render(out, (n + "_held").c_str(), h.id, L, {});
            render(out, (n + "_thrown_out").c_str(), h.id, L, throwAt(props::shield, props::shieldThrow, .25f, true));
            render(out, (n + "_coming_back").c_str(), h.id, L, throwAt(props::shield, props::shieldThrow, .62f, true));
            props::Flying close; // the shield's face, 1.4 m ahead (its face is toward its way's side: a look at the star)
            close.model = props::shield, close.middle = {0, 1.45f, -1.4f}, close.way = normalized(Vec3{0, .3f, 1});
            render(out, (n + "_face").c_str(), h.id, L, close);
        } else if (h.id == hero_modes::batman) {
            render(out, (n + "_batarang").c_str(), h.id, L, throwAt(props::batarang, props::batarangThrow, .07f, false));
            props::Flying close; // a close look (no spin), 0.8 m ahead and a little under the eyes
            close.model = props::batarang, close.middle = {.05f, 1.35f, -.8f}, close.way = {0, 0, -1};
            render(out, (n + "_batarang_close").c_str(), h.id, L, close);
        } else {
            render(out, (n + "_held").c_str(), h.id, L, {});
        }
        ++drawn;
    }
    return drawn ? 0 : 1;
}
