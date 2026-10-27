#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Version 0.0.7 BATARANG STYLE / GUN MODELS / BATON LOOK: a small, strict glTF 2.0 reader for the
// drop-in models (Sketchfab models, credited in THIRD-PARTY-MODELS.md; the release zip carries
// them in its Models folder; see docs/HERO-0.0.7.md). Portable (no
// Windows), tested in core_tests.
//
// THE EXPECTED FILES (Sketchfab's "glTF" download, unzipped, or one .glb):
//   <Spidy folder>\Models\Batarang\<STYLE>\scene.gltf + scene.bin (+ textures\)
//   or <Spidy folder>\Models\Batarang\<STYLE>\scene.glb (or model.glb)
//   STYLE: EIRA, CLOONEY, JOKER. The guns and batons: Models\Pistol, Models\SawedOff\Frame|Barrels,
//   Models\Baton\Left|Right, Models\Shield (each a scene.glb from tools/models007/convert_models.py).
// Read: every node of the default scene (its matrix or translation / rotation / scale, down the
// tree), each mesh primitive in TRIANGLES mode (4, the default), POSITION (FLOAT VEC3) and indices
// (UNSIGNED_BYTE / SHORT / INT, or none), COLOR_0 (the converter's baked colours) when present, each
// material's pbrMetallicRoughness.baseColorFactor, asset.extras.pivot (a hinge), and (0.0.72 REAL
// MODELS) NORMAL, TEXCOORD_0 and each material's baseColorTexture (PNG / JPEG / QOI, decoded by the
// host's Decoder, packed by prepareTextures into one texture array the overlay samples; alphaMode
// MASK is kept). A model whose texture fails is drawn in that texture's mean colour, never white.
// Not read: animation, skins. Refused (the batarang stays DEFAULT): a missing or
// unreadable file, a bad magic / version / length, a JSON error, Draco or meshopt compression (an
// extensionsRequired), an accessor or view out of the buffer, an index out of range, a NaN or a huge
// coordinate, no triangles, more than mostSourceTriangles, a file over mostFileBytes.
//
// Nothing is ever written back. Scale, centring and the turn to the hand are applied when drawing
// (fit), never baked into the mesh. Decimation (vertex clustering, a fallback: the converter already
// brings every CC BY model to drawTriangles) only where a model's licence allows changes; JOKER
// (CC BY-NC-ND) is drawn unmodified or not at all.
#include "spidy/math.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace spidy::mesh_model {

inline constexpr size_t mostFileBytes = 64u << 20;
inline constexpr uint32_t mostSourceTriangles = 200000;
// Version 0.0.72 REAL MODELS: the models keep their real geometry (tools/models072: whole, or
// collapse-decimated only where VR needs it; the largest ships at about 14,000), so the per-model cap
// is 24000 (was 700, which crushed them to flat shapes).
inline constexpr uint32_t drawTriangles = 24000; // the VR cap per model (3 vertices each)
// Untextured (the overlay could not make its texture array), each model is drawn sorted far to near
// in the overlay's own vertices: then its light copy (Mesh::lite, vertex clustering to liteTriangles).
inline constexpr uint32_t liteTriangles = 2400;
// Version 0.0.72 REAL MODELS: the base-colour textures. Each model image (a GLB bufferView or a file
// beside a .gltf, PNG / JPEG / QOI) is decoded once at start (QOI here; PNG / JPEG by the decoder
// game_xr gives: WIC), scaled to textureSize x textureSize and put in one layer of the overlay's
// texture array (D3D12Renderer::setModelTextures). Equal images share a layer.
inline constexpr uint32_t textureSize = 1024, textureLevels = 11, mostTextureLayers = 24;
inline constexpr size_t mostImageBytes = 48u << 20;

struct Mesh {
    std::vector<Vec3> corners; // 3 per triangle, model space (metres as the file says, any scale)
    std::vector<Vec3> colors;  // 1 per triangle (linear RGB, the material's base colour)
    Vec3 low{}, high{};        // the bounds
    uint32_t sourceTriangles{};
    bool decimated{};
    Vec3 pivot{};     // asset.extras.pivot (the sawed-off barrels' hinge), model space
    bool hasPivot{};
    // Version 0.0.7 CAPTAIN AMERICA: asset.extras.layer [split, centre xyz, normal xyz] (the shield):
    // triangles [0, layerSplit) are the face's disc, [layerSplit, n) the straps on its back;
    // layerCentre / layerNormal the back's centre and the face's outward normal (model space). The
    // drawer puts the side nearer the eye last. Dropped (hasLayer false) when decimated.
    uint32_t layerSplit{};
    Vec3 layerCentre{}, layerNormal{};
    bool hasLayer{};
    // Version 0.0.72 REAL MODELS (all empty for an untextured model):
    std::vector<float> uvs;     // 6 a triangle (u v per corner, TEXCOORD_0) when every primitive has them
    std::vector<Vec3> normals;  // 3 a triangle (NORMAL, model space) when every primitive has them
    std::vector<Vec3> tints;    // 1 a triangle: the material's baseColorFactor (multiplies the texture)
    std::vector<int16_t> image; // 1 a triangle: its base-colour image (index into images), -1 none
    struct Image {
        std::string uri;            // a file beside the .gltf (read by loadFolder), or empty
        std::vector<uint8_t> bytes; // the encoded image (PNG / JPEG / QOI)
        bool cutout = false;        // alphaMode MASK / BLEND: alpha under .5 is cut
        int32_t layer = -1;         // its layer in the overlay's texture array (Library::prepareTextures)
    };
    std::vector<Image> images;
    std::shared_ptr<const Mesh> lite; // Library::load: a copy at most liteTriangles (when this is over), untextured
    uint32_t triangles() const { return static_cast<uint32_t>(colors.size()); }
    bool ok() const { return !colors.empty() && corners.size() == colors.size() * 3; }
    bool hasUvs() const { return uvs.size() == colors.size() * 6 && !uvs.empty(); }
    bool hasNormals() const { return normals.size() == colors.size() * 3 && !normals.empty(); }
    // Drawn textured: UVs, and every triangle with an image has its layer.
    bool textured() const;
};

// Parse one file's bytes. gltfJson: a .gltf's text with its .bin (bin may be empty when the buffer
// is a data: URI is NOT supported - refused). glb: a whole .glb. False with `error` set on any problem.
bool parseGltf(const std::string& json, const std::vector<uint8_t>& bin, Mesh& out, std::string& error);
bool parseGlb(const uint8_t* data, size_t size, Mesh& out, std::string& error);

// Vertex clustering to at most `most` triangles (a grid over the bounds, finer until it fits).
// Returns false (mesh unchanged) when it cannot get under `most`.
bool decimate(Mesh& mesh, uint32_t most);

// Load a style folder (path ends with a slash or backslash; tries scene.glb, model.glb, scene.gltf).
// mayDecimate false: a mesh over drawTriangles is refused instead of decimated (JOKER, ND).
enum Status : uint32_t { notTried = 0, loaded, missing, invalid, overBudget };
inline constexpr const char* statusNames[] = {"NOT TRIED", "LOADED", "MISSING", "INVALID", "OVER BUDGET (NO CHANGES ALLOWED)"};
Status loadFolder(const std::string& folder, bool mayDecimate, Mesh& out, std::string& error);
bool readFile(const std::string& path, std::vector<uint8_t>& out, size_t most = mostFileBytes);

// ---- BATARANG STYLE and the other drop-in models ---------------------------------------------------
// Slots under <Spidy folder>\Models\ (tools/models007/convert_models.py writes them; the CC BY ones
// are already converted: metres, x right, y up, -z forward, the grip at the origin, colours baked).
enum Style : uint32_t { styleDefault = 0, styleEira, styleClooney, styleJoker, styles };
// Version 0.0.7 CAPTAIN AMERICA: slotShield (Models\Shield, CC BY, tools/models007/convert_shield.py).
// Version 0.0.7 BATMAN: slotGrappleGun (Models\GrappleGun): the grapple gun (Falxx, CC BY 4.0), its grip middle at the
// origin and its muzzle as extras.pivot. Missing: no gun is drawn (no stand-in) and the line starts at the hand.
enum Slot : uint32_t { slotPistol = styles, slotSawedOffFrame, slotSawedOffBarrels, slotBatonLeft, slotBatonRight, slotShield, slotGrappleGun, slots };
inline constexpr const char* slotFolders[slots] = {"",       "Batarang/EIRA",     "Batarang/CLOONEY", "Batarang/JOKER",
                                                   "Pistol", "SawedOff/Frame",    "SawedOff/Barrels", "Baton/Left",
                                                   "Baton/Right", "Shield",           "GrappleGun"};
// JOKER (CC BY-NC-ND): drawn exactly as downloaded or not at all.
inline constexpr bool slotMayDecimate[slots] = {false, true, true, false, true, true, true, true, true, true, true};
// The models loaded this session, kept for the whole session (never freed: frames in flight may
// point at them). load() tries a slot's folder once; model() is null when it is DEFAULT, missing
// or refused - the built-in look is then drawn instead.
struct Library {
    Mesh meshes[slots];
    Status status[slots]{};
    std::string error[slots];
    Status load(uint32_t slot, const std::string& modelsRoot); // modelsRoot: ...\Models\ (with the slash)
    const Mesh* model(uint32_t slot) const {
        return slot > 0 && slot < slots && status[slot] == loaded && meshes[slot].ok() ? &meshes[slot] : nullptr;
    }
};
Library& library(); // the session's one

// ---- Version 0.0.72 REAL MODELS: the texture array --------------------------------------------------
struct Picture {
    uint32_t width{}, height{};
    std::vector<uint8_t> rgba; // rows from the top
};
// Decodes PNG / JPEG (game_xr: WIC); null or false: only QOI images are read.
using Decoder = bool (*)(const uint8_t* data, size_t size, Picture& out);
bool decodeImage(const uint8_t* data, size_t size, Decoder decoder, Picture& out);
// Scales `in` to size x size (a box filter down, bilinear up), sRGB kept.
Picture resample(const Picture& in, uint32_t size);
struct TextureArray {
    uint32_t size = textureSize, levels = textureLevels;
    std::vector<std::vector<uint8_t>> layers; // each: its mips, level 0 first, size >> level square, RGBA8 sRGB
    std::vector<std::string> notes;           // one line an image: its slot, name, layer or why it was not read
};
// Every loaded slot's images, decoded (equal bytes once), scaled, mipped, given a layer (Image::layer);
// each textured triangle's colors[] becomes its texture's mean (linear) x tint, the look used when the
// overlay cannot texture. An image that fails leaves its triangles in their tint (never a crash).
// `size` and `levels` (tests use small ones) must satisfy size >> (levels - 1) >= 1.
TextureArray prepareTextures(Library& library, Decoder decoder, uint32_t size = textureSize, uint32_t levels = textureLevels);

// Where the model sits on a hand-held / thrown batarang: centred on its bounds, its longest side
// across (right), its thinnest side up (the blade's normal), scaled so the span is `span` metres.
struct Fit {
    Vec3 centre{};
    int across = 0, thin = 2, along = 1; // bounding-box axes 0 x, 1 y, 2 z
    float scale = 0;
};
Fit fit(const Mesh& mesh, float span);

} // namespace spidy::mesh_model
