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
// Version 0.0.72 THOR HAMMERS + PUNISHER VAN: a slot's own cap (slotDrawCap): the hammers 50,000 (a hero prop held
// at the eye: the user's "make sure the new models arent falttened"), the van 150,000 (a whole vehicle; the source
// has 431,452, the shipped one 135,000 by collapse decimation). Every other slot keeps drawTriangles.
inline constexpr uint32_t hammerTriangles = 50000, vanTriangles = 150000;
// Version 0.0.74 test 1 HIDEOUTS: a whole room (the Batcave ships 40,533).
inline constexpr uint32_t hideoutTriangles = 60000;
// Untextured (the overlay could not make its texture array), each model is drawn sorted far to near
// in the overlay's own vertices: then its light copy (Mesh::lite, vertex clustering to liteTriangles).
inline constexpr uint32_t liteTriangles = 2400;
// Version 0.0.72 REAL MODELS: the base-colour textures. Each model image (a GLB bufferView or a file
// beside a .gltf, PNG / JPEG / QOI) is decoded once at start (QOI here; PNG / JPEG by the decoder
// game_xr gives: WIC), scaled to textureSize x textureSize and put in one layer of the overlay's
// texture array (D3D12Renderer::setModelTextures). Equal images share a layer.
// Version 0.0.72 THOR HAMMERS + PUNISHER VAN: 32 layers (the four hammers and the van take 15; about 160 MB at 1024 px with mips).
// Test build 8 ROSTER page: 40 (the roster's six picture atlases on top of the 28 images; the game uses
// 512 px layers - about 1.4 MB each with mips - so the roster adds about 8 MB).
// Build 9 SUITS: 112 (the 23 suits take at most 3 layers each, 69 at most; the game's 512 px layers: about 160 MB).
// 0.0.74 test 1 HIDEOUTS: 160 (the Batcave's 35 images and the safehouse's; loaded only when HIDEOUTS is ON).
// 0.0.74 test 1 BATMOBILE: 192 (the four cars, the cockpit and the wheel bring 17 images; ON only).
inline constexpr uint32_t textureSize = 1024, textureLevels = 11, mostTextureLayers = 192;
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
    // 0.0.74 test 2 CAR PAINT (car_paint.hpp): 2 a triangle (metallic, roughness, 0..255) for an untextured triangle
    // whose material says metallicFactor >= car_paint::paintFrom (0 0 otherwise); kept only for the Batmobile
    // bodies and roofs (slotIsPaintable), empty for every other model.
    std::vector<uint8_t> paint;
    // 0.0.74 test 3 COCKPIT GLOW / DASH INSIDE (cabin_look074.hpp): 1 a triangle, its material's role (a lamp, a
    // switch tip, the baked handle marker; 0 plain); kept only for the Batmobile slots, empty when none has one.
    // cabin: the Batmobile cockpit or its steering wheel (darkened and lit by COCKPIT GLOW).
    std::vector<uint8_t> role;
    bool cabin{};
    struct Image {
        std::string uri;            // a file beside the .gltf (read by loadFolder), or empty
        std::vector<uint8_t> bytes; // the encoded image (PNG / JPEG / QOI)
        bool cutout = false;        // alphaMode MASK / BLEND: alpha under .5 is cut
        int32_t layer = -1;         // its layer in the overlay's texture array (Library::prepareTextures)
    };
    std::vector<Image> images;
    // Build 9 SUITS (suit_wear.hpp; tools/suits/convert_suit.py): JOINTS_0 / WEIGHTS_0, 4 a corner (12 a triangle):
    // suit_wear's 16 bones, the weights summing to 255. Kept only when every primitive has both.
    std::vector<uint8_t> skinJoints, skinWeights;
    // asset.extras.suit: the rest skeleton (suit_wear::ends: the 16 joints, the head's top, the fingertips; metres,
    // +y up, facing +z, its left at +x) and the backs of the hands (left xyz, right xyz). hasSuit: all of it read.
    float suitEnds[19 * 3]{};
    float suitHandBack[6]{};
    bool hasSuit{};
    uint32_t cap = drawTriangles; // Version 0.0.72: its slot's draw cap (loadFolder; slotDrawCap)
    std::shared_ptr<const Mesh> lite; // Library::load: a copy at most liteTriangles (when this is over), untextured
    uint32_t triangles() const { return static_cast<uint32_t>(colors.size()); }
    bool ok() const { return !colors.empty() && corners.size() == colors.size() * 3; }
    bool hasUvs() const { return uvs.size() == colors.size() * 6 && !uvs.empty(); }
    bool hasNormals() const { return normals.size() == colors.size() * 3 && !normals.empty(); }
    bool hasSkin() const {
        return !skinJoints.empty() && skinJoints.size() == colors.size() * 12 && skinWeights.size() == skinJoints.size();
    }
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
// Version 0.0.72: cap, the slot's draw cap (slotDrawCap; drawTriangles by default), is kept in Mesh::cap.
Status loadFolder(const std::string& folder, bool mayDecimate, Mesh& out, std::string& error, uint32_t cap = drawTriangles);
bool readFile(const std::string& path, std::vector<uint8_t>& out, size_t most = mostFileBytes);

// ---- BATARANG STYLE and the other drop-in models ---------------------------------------------------
// Slots under <Spidy folder>\Models\ (tools/models007/convert_models.py writes them; the CC BY ones
// are already converted: metres, x right, y up, -z forward, the grip at the origin, colours baked).
enum Style : uint32_t { styleDefault = 0, styleEira, styleClooney, styleJoker, styles };
// Version 0.0.7 CAPTAIN AMERICA: slotShield (Models\Shield, CC BY, tools/models007/convert_shield.py).
// Version 0.0.7 BATMAN: slotGrappleGun (Models\GrappleGun): the grapple gun (Falxx, CC BY 4.0), its grip middle at the
// origin and its muzzle as extras.pivot. Missing: no gun is drawn (no stand-in) and the line starts at the hand.
// Version 0.0.72 THOR HAMMERS (the user's picks 1, 5, 6, 3, CC BY 4.0; THOR HAMMER selects, no restart): slotHammer
// (Models\Hammer\BOOOOP, the default), slotHammerDevilsEye, slotHammerLoveThunder, slotHammerFccooper. Each: the
// handle's end at the origin, +y the handle -> head, -z a face, 50 cm long. Missing: no hammer is drawn and the
// wrist says MODEL MISSING: Hammer/<name> (no stand-in).
// Version 0.0.72 PUNISHER VAN (EXPERIMENTAL, van072.hpp; owner pick pt850u "For the van do 8": the GMC Vandura
// 1976 by snafuj, CC BY 4.0): slotVan (Models\Vehicle\PunisherVan) the body, metres, -z forward, x right, the
// ground under the axles' middle at the origin; slotVanInterior (.../Interior) the cab BUILT for the empty source
// shell (the same frame); slotVanWheel (.../SteeringWheel) the hub at the origin, the rim in the xy plane, +z
// toward the driver, +y 12 o'clock (turned by the hands).
// Test build 8 ROSTER page: slotRoster (Models\Roster, tools/roster/make_roster_glb.py) is not a model that is
// drawn: it carries the ROSTER page's profile pictures and suit previews (picture k = triangles 2k, 2k+1, their
// UVs its box in an atlas); roster_page draws them as flat quads. Missing: the page shows PICTURE MISSING.
// Build 9 SUITS: slotSuitFirst + k (Models\Suits\<suitFolders[k]>, tools/suits/convert_suit.py) the roster's suits,
// skinned (Mesh::hasSkin, hasSuit) and worn by suit_wear (the mirror, the first-person arms / body). Never decimated
// here (the converter ships at most drawTriangles: collapse decimation, the shape kept). Missing: the suit is
// listed MODEL MISSING and the game's own look stays.
inline constexpr uint32_t suitSlots = 23;
inline constexpr const char* suitFolders[suitSlots] = {
    "Suit_batman_1",  "Suit_batman_2",         "Suit_batman_3",         "Suit_batman_5",   "Suit_ironMan_1",   "Suit_ironMan_2",
    "Suit_ironMan_3", "Suit_ironMan_4",        "Suit_captainAmerica_1", "Suit_captainAmerica_2", "Suit_daredevil_1", "Suit_daredevil_3",
    "Suit_hulk_2",    "Suit_wolverine_1",      "Suit_wolverine_3",      "Suit_wolverine_4", "Suit_punisher_1", "Suit_punisher_2",
    "Suit_thor_1",    "Suit_thor_2",           "Suit_greenLantern_2",   "Suit_greenLantern_3", "Suit_superman_4"};
enum Slot : uint32_t { slotPistol = styles, slotSawedOffFrame, slotSawedOffBarrels, slotBatonLeft, slotBatonRight, slotShield, slotGrappleGun, slotHammer, slotVan,
                       slotVanInterior, slotVanWheel, slotHammerDevilsEye, slotHammerLoveThunder, slotHammerFccooper, slotRoster, slotSuitFirst,
                       slotSuitEnd = slotSuitFirst + suitSlots,
                       // Version 0.0.73 (fix 10) RETRACTABLE CLAWS: Models\Claws\Left|Right (tools/claws073/extract_claws.py: the
                       // JACKET AND JEANS Wolverine's own claws, Light.k, CC BY 4.0), the claws' base at the origin, -z along
                       // them, +y the back of the hand. Missing: 0.0.72's built-in claws.
                       slotClawsLeft = slotSuitEnd, slotClawsRight,
                       // Version 0.0.74 test 1 HIDEOUTS: Models\Hideout\Batcave|Safehouse (tools/hideout074/make_hideout.py: the
                       // room in glTF space, its room.txt beside it). Loaded only when HIDEOUTS is ON (game_xr).
                       slotHideoutBatcave, slotHideoutSafehouse,
                       // 0.0.74 test 1 BATMOBILE (batmobile074.hpp; owner picks 01 / 07 / 09 / 10): Models\Vehicle\Batmobile:
                       // <car> the body (the van's frame: metres, -z forward, x right, the ground at y 0; its glass as the model
                       // came), Cockpit the shared cockpit (its own frame: the
                       // floor's middle at the origin; placed per car), SteeringWheel (the van wheel's convention). Loaded
                       // only when BATMOBILE is ON (game_xr). Missing: no Batmobile (the van's X stays the Punisher's).
                       slotBatmobileFirst, slotBatmobileCockpit = slotBatmobileFirst + 4,
                       slotBatmobileWheel,
                       // 0.0.74 test 2 INSIDE VIEW CULL: <car>\Roof (the canopy / roof / cabin glass the converter cut out of
                       // the body over the cockpit; drawn only from outside the car). Missing: the car without it.
                       slotBatmobileRoofFirst,
                       slots = slotBatmobileRoofFirst + 4 };
inline constexpr const char* slotFolders[slots] = {"",       "Batarang/EIRA",     "Batarang/CLOONEY", "Batarang/JOKER",
                                                   "Pistol", "SawedOff/Frame",    "SawedOff/Barrels", "Baton/Left",
                                                   "Baton/Right", "Shield",           "GrappleGun",       "Hammer/BOOOOP",
                                                   "Vehicle/PunisherVan", "Vehicle/PunisherVan/Interior", "Vehicle/PunisherVan/SteeringWheel",
                                                   "Hammer/DEVILSEYE", "Hammer/LOVEANDTHUNDER", "Hammer/FCCOOPER", "Roster",
                                                   "Suits/Suit_batman_1", "Suits/Suit_batman_2", "Suits/Suit_batman_3", "Suits/Suit_batman_5",
                                                   "Suits/Suit_ironMan_1", "Suits/Suit_ironMan_2", "Suits/Suit_ironMan_3", "Suits/Suit_ironMan_4",
                                                   "Suits/Suit_captainAmerica_1", "Suits/Suit_captainAmerica_2", "Suits/Suit_daredevil_1",
                                                   "Suits/Suit_daredevil_3", "Suits/Suit_hulk_2", "Suits/Suit_wolverine_1", "Suits/Suit_wolverine_3",
                                                   "Suits/Suit_wolverine_4", "Suits/Suit_punisher_1", "Suits/Suit_punisher_2", "Suits/Suit_thor_1",
                                                   "Suits/Suit_thor_2", "Suits/Suit_greenLantern_2", "Suits/Suit_greenLantern_3", "Suits/Suit_superman_4",
                                                   "Claws/Left", "Claws/Right", "Hideout/Batcave", "Hideout/Safehouse",
                                                   "Vehicle/Batmobile/1989", "Vehicle/Batmobile/ODAMI", "Vehicle/Batmobile/FOREVER",
                                                   "Vehicle/Batmobile/FOREVER2", "Vehicle/Batmobile/Cockpit",
                                                   "Vehicle/Batmobile/SteeringWheel", "Vehicle/Batmobile/1989/Roof",
                                                   "Vehicle/Batmobile/ODAMI/Roof", "Vehicle/Batmobile/FOREVER/Roof",
                                                   "Vehicle/Batmobile/FOREVER2/Roof"};
inline constexpr uint32_t slotDrawCap[slots] = {drawTriangles, drawTriangles, drawTriangles, drawTriangles, drawTriangles, drawTriangles,
                                                drawTriangles, drawTriangles, drawTriangles, drawTriangles, drawTriangles, hammerTriangles,
                                                vanTriangles, drawTriangles, drawTriangles, hammerTriangles, hammerTriangles, hammerTriangles, drawTriangles,
                                                drawTriangles, drawTriangles, drawTriangles, drawTriangles, drawTriangles, drawTriangles, drawTriangles,
                                                drawTriangles, drawTriangles, drawTriangles, drawTriangles, drawTriangles, drawTriangles, drawTriangles,
                                                drawTriangles, drawTriangles, drawTriangles, drawTriangles, drawTriangles, drawTriangles, drawTriangles,
                                                drawTriangles, drawTriangles, drawTriangles, drawTriangles, hideoutTriangles, hideoutTriangles,
                                                vanTriangles, vanTriangles, vanTriangles, vanTriangles, drawTriangles, drawTriangles,
                                                drawTriangles, drawTriangles, drawTriangles, drawTriangles};
// THOR HAMMER's choice (combat38::thorHammer: 0 BOOOOP, 1 DEVILSEYE, 2 LOVE+THUNDER, 3 FCCOOPER) -> its slot.
inline constexpr uint32_t hammers = 4;
inline constexpr uint32_t hammerSlot(uint32_t choice) {
    return choice == 1 ? slotHammerDevilsEye : choice == 2 ? slotHammerLoveThunder : choice == 3 ? slotHammerFccooper : slotHammer;
}
// The van's parts (drawn by hero007::appendPlacedModel: up to 8 m, seen to 80 m).
inline constexpr bool slotIsBatmobile(uint32_t slot) { return slot >= slotBatmobileFirst && slot < slotBatmobileRoofFirst + 4; }
// 0.0.74 test 2 CAR PAINT: the four Batmobile bodies and their roofs. 0.0.74 test 3 (owner: "satin black metallic
// instead of the stretched zigzag texture"): the cockpit too (its trim is now untextured satin black).
inline constexpr bool slotIsPaintable(uint32_t slot) {
    return (slot >= slotBatmobileFirst && slot < slotBatmobileFirst + 4) || (slot >= slotBatmobileRoofFirst && slot < slotBatmobileRoofFirst + 4) ||
           slot == slotBatmobileCockpit;
}
// 0.0.74 test 3 COCKPIT GLOW: the models COCKPIT GLOW darkens and lights (the cockpit and its wheel).
inline constexpr bool slotIsCabin(uint32_t slot) { return slot == slotBatmobileCockpit || slot == slotBatmobileWheel; }
inline constexpr bool slotIsVehicle(uint32_t slot) {
    return slot == slotVan || slot == slotVanInterior || slot == slotVanWheel || slotIsBatmobile(slot);
}
// JOKER (CC BY-NC-ND): drawn exactly as downloaded or not at all.
// Build 9: the suits never (vertex clustering would lose their skin and flatten them).
inline constexpr bool slotMayDecimate[slots] = {false, true, true, false, true, true, true, true, true, true, true, true, true, true, true, true, true, true, false};
inline constexpr bool slotIsSuit(uint32_t slot) { return slot >= slotSuitFirst && slot < slotSuitEnd; }
// A suit's slot by its folder ("Suit_batman_1"), 0 when it is not one.
inline uint32_t suitSlot(const char* folder) {
    if (!folder)
        return 0;
    for (uint32_t k = 0; k < suitSlots; ++k) {
        const char *a = suitFolders[k], *b = folder;
        while (*a && *a == *b)
            ++a, ++b;
        if (!*a && !*b)
            return slotSuitFirst + k;
    }
    return 0;
}
inline constexpr bool slotIsHideout(uint32_t slot) { return slot == slotHideoutBatcave || slot == slotHideoutSafehouse; }
static_assert(slotSuitFirst == 19 && slotSuitEnd == 42 && slotHideoutBatcave == 44 && slotBatmobileFirst == 46 &&
                  slotBatmobileWheel == 51 && slotBatmobileRoofFirst == 52 && slots == 56, "the slot list (slotFolders, slotDrawCap, slotMayDecimate) follows it");
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
