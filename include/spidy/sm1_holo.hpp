#pragma once
// HERO VR 0.0.76 SM1: the add-on's hologram (the user, on the first SM1 wrist menu: "the wrist menu is awful it should
// look exactly like sm2 holo does"). Made by Agent Mulder.
//
// The SM2 build draws its hologram as triangles into its own overlay. In Spider-Man 1 the add-on cannot draw into
// iilyn's frame (his eye images and rendering stay his), so it puts a quad layer on top of his frame instead. This
// module turns the SM2 hologram into that quad's picture: wrist_hud::appendPanelFlat lays the SAME panel out (its
// painter, font, palette, roster tiles, portraits, tabs and COLOR rows) flat, and raster() fills those triangles on
// the CPU into a Canvas the layer uploads. Nothing is redrawn while the picture is the same (hashOf).
//
// Pure C++ (no Windows, no GPU): unit tested in tests/sm1_addon_tests.cpp.
#include "spidy/holo_roster.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/roster_page.hpp"
#include "spidy/sm1_addon.hpp"
#include "spidy/vertex.hpp"
#include "spidy/wrist_hud.hpp"

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace spidy::sm1::holo {
// The picture's piece of the panel (panel metres, the panel's centre at 0, 0): the panel (18 x 22 cm), its frame and
// glow, at `pixel` metres a pixel.
inline constexpr float halfWidth = .096f, halfHeight = .126f, pixel = .00025f;
inline constexpr int width = 768, height = 1008; // 2 * halfWidth / pixel, 2 * halfHeight / pixel
// The PLAYING AS note (after PLAY AS): its own quad in front of the player.
inline constexpr float noteHalfWidth = .13f, noteHalfHeight = .05f, notePixel = .0004f;
inline constexpr int noteWidth = 650, noteHeight = 250;
// The pointer's dot.
inline constexpr int dotSize = 32;

// The portraits (Models/Roster/scene.glb, the SM2 holo's): its images decoded (RGBA8 sRGB, rows from the top),
// image i at layer i, and where each picture is on them.
struct Portraits {
    roster_page::Picture at[roster_page::pictureCount]{};
    std::vector<mesh_model::Picture> layers;
    bool ok{};
};
// Reads the Roster model and decodes its images with `decoder` (the layer: WIC). False with `why` when it cannot;
// the tiles then show the SM2 holo's initials discs.
bool loadPortraits(const uint8_t* glb, size_t size, mesh_model::Decoder decoder, Portraits& out, std::string& why);

// Fills `translucent` (each vertex's linear colour and alpha, blended over in order) and then `textured` (a
// portrait: shape (u, v, layer, 1), its colour the tint) into `out` (out.width x out.height, cleared first), the
// area (-halfW..halfW) x (-halfH..halfH) metres; 0xAABBGGRR, sRGB, straight alpha (the layer's upload premultiplies).
void raster(const std::vector<Vertex>& translucent, const std::vector<Vertex>& textured, const Portraits* portraits,
            float halfW, float halfH, Canvas& out);
// A key of the triangles (what raster would draw): equal keys, equal pictures.
uint64_t hashOf(const std::vector<Vertex>& translucent, const std::vector<Vertex>& textured);

// The panel's triangles for `view` (no pointer: the layer draws its dot on its own quad, so pointing does not redraw).
void panel(const wrist_hud::View& view, std::vector<Vertex>& translucent, std::vector<Vertex>& textured);
// The PLAYING AS note's triangles.
void note(const std::string& title, const std::vector<std::string>& lines, std::vector<Vertex>& out);
// The pointer's dot (dotSize square): a bright ring in the holo's accent.
void paintDot(Canvas& c);

// --- What the SM1 holo shows and works -----------------------------------------------------------------------------
// The switches on a hero's POWERS tab (holo_roster::Sm1Switch::key).
enum Switch : int { swPowers = 0, swBeams, swFlight, swGrapple, swHeroWebs, switchCount };
const char* switchName(int key);
struct Now {
    Config config{};
    Live live{};
    bool flightSwitch = true;    // the add-on's FLIGHT switch (this session)
    bool flying = false;
    bool powersWork = true, flightWorks = true, grappleWorks = true; // each feature's own check (off: its fault)
    std::string powerState;      // the last shot's ("HIT", "NO TARGET" ...)
    std::string why[3];          // why powers, flight, grapple are off ("" none)
    std::string flightLine;      // the layer's flight status ("FLIGHT: ARMS UP OR HOLD A" when his A is bound; "" unknown)
    std::string suitLine;        // the launcher's hero suit for the hero picked (suitHint: "SUIT: PICK ... IN THE GAME'S SUITS MENU")
};
bool grapples(uint32_t hero); // CAPTAIN AMERICA, DAREDEVIL
// The roster's Data in SM1 (sm1.on): every hero's tile with its power, its CONTROLS lines (only what works in SM1),
// its POWERS switches, OUTFITS from HERO VR's roster with the note that the pick is saved only.
holo_roster::Data rosterData(const Now& now);
// The note after PLAY AS: what the hero does now, in a few lines.
std::vector<std::string> playLines(uint32_t hero, const Now& now);
// COLOR: the SM2 sliders from live.txt and back (the SM2 build's own mapping), and View::color's bits.
wrist_hud::Settings colorSettings(const Live& live);
void takeColor(const wrist_hud::Settings& s, Live& live);
uint32_t colorBits(const Live& live);
// The next look for FILTER (SM2's order; 90s ANIMATED is SM2's own shader, skipped in SM1).
uint32_t nextFilter(uint32_t filter);

// --- A thread of its own for raster() (a hover redraw never stalls a frame) ---------------------------------------
class Painter {
  public:
    Painter();
    ~Painter();
    Painter(const Painter&) = delete;
    Painter& operator=(const Painter&) = delete;
    // The portraits (kept: they are read while painting).
    void setPortraits(std::shared_ptr<const Portraits> p);
    // Asks for a picture (the newest ask wins; one already asked with this key is not asked again).
    void ask(uint64_t key, std::vector<Vertex> translucent, std::vector<Vertex> textured, float halfW, float halfH, int w,
             int h);
    // The newest finished picture, once (false: none since the last take).
    bool take(Canvas& out, uint64_t& key);
    // Stops the thread (the destructor does too).
    void stop();

  private:
    void run();
    std::mutex m_;
    std::condition_variable cv_;
    struct Job {
        uint64_t key{};
        std::vector<Vertex> translucent, textured;
        float halfW{}, halfH{};
        int w{}, h{};
    };
    Job job_{};
    bool pending_{}, done_{}, quit_{};
    uint64_t asked_{};
    Canvas result_{};
    uint64_t resultKey_{};
    std::shared_ptr<const Portraits> portraits_;
    std::thread thread_;
};
} // namespace spidy::sm1::holo
