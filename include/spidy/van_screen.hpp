#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.73 test 3 VAN SCREEN (owner pt1017u): a small dash screen in the PUNISHER van, a gritty
// military "war journal" look with an original skull (no copied logo). Its pages:
//   MAP     a stylized heading display: the van's own heading (its frame's yaw) and speed over a
//           decorative street grid that scrolls with the van (NO game map: the mod reads none).
//   STATUS  speed (MPH / KM/H), ARMOR: BULLETPROOF, the engine (the van's own rpm), gear.
//   CASES   seven short case files (original text): Jigsaw, Kingpin, Bullseye, Barracuda, the
//           Russian, Ma Gnucci, Microchip.
//   LOCKER  the weapons locker (flavour text).
// It is built into the dash face, centre dash right of the wheel, flush and square to the van (MEASURED
// on the built cab; in game CANDIDATE). Paged by X while driving (X is the van's and does nothing then)
// or by a poke with an open hand (grip under pokeGripMost, not on the wheel), or (RAY POKE) by the free
// hand pointed at it and its trigger pulled: the tab row picks a
// page; on CASES the left third goes back a file, the rest on. It never takes the wheel's grip or the
// triggers. Pure: no game reads, no addresses. Everything it draws is checked: anything non-finite
// (or a throw) and it draws nothing at all.
#include "spidy/math.hpp"
#include "spidy/van072.hpp"
#include "spidy/vertex.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace spidy::van_screen {

// The wrist option's one-sentence help (combat38 VAN SCREEN; ON is value 0, the default).
inline constexpr const char* help = "Shows a Punisher dash screen in the van - X or a poke pages it, no restart.";

// 0.0.74 test 1: + MUSIC (the vehicle music player: previous, play / pause, next, volume, the song) and HOME
// (GO TO SAFEHOUSE, GO TO BATCAVE, SIT / STAND). Their buttons act on a poke or a ray + trigger (Screen::action);
// a tap off a button does nothing there.
enum Page : uint32_t { map, status, cases, locker, music, home };
inline constexpr uint32_t pages = 6;
inline constexpr const char* pageNames[pages] = {"MAP", "STATUS", "CASES", "LOCKER", "MUSIC", "HOME"};
// The tab row's k-th tab's centre (x, design units; the skull's corner left of the first).
inline constexpr float tabLeft = .034f;
// 0.0.74 test 1: what a button asks for (the caller does it; the screen only says).
enum class Action : uint8_t { none, musicPrevious, musicToggle, musicNext, volumeDown, volumeUp, goSafehouse, goBatcave, sit, stand };
// Which of the 0.0.74 buttons are there (their wrist options): hidden ones neither draw nor act.
struct Offer {
    bool music = true;     // MUSIC PLAYER not OFF
    bool hideouts = true;  // HIDEOUTS ON
    bool seat = true;      // SEATED DRIVING not OFF
    bool sat = false;      // seated now: the seat button reads STAND
};
struct Button {
    Action action{};
    float x0{}, y0{}, x1{}, y1{}; // design units on the face
    const char* label{};
};
// The page's buttons (MUSIC, HOME; none elsewhere) into `out`; returns how many (at most 8).
uint32_t buttons(uint32_t page, const Offer& offer, Button out[8]);

struct CaseFile {
    const char* name;
    const char* alias;
    const char* threat; // LOW .. EXTREME
    uint32_t level;     // 1..5 bars
    const char* bio;    // original, short
};
inline constexpr uint32_t files = 7;
extern const CaseFile caseFiles[files];

// The screen's size and place in the van's frame (van072: x right, y up, -z forward, the ground under
// the axles' middle). Built into the dash face (the user, 0.0.73 test 3: "flush with the dash ... facing
// straight not an angle"): parallel to the fascia, square to the van (no tilt, no yaw, no pitch), its face
// toward the cab's back (+z). MEASURED against the built cab's triangles (the Interior model as the game
// places it): the dash's driver-side face is the plane z -1.28 (normal +z), flat from y .98 to 1.25, the
// cluster hood's side x -.18..-.19 above y 1.23, a part's side at x .25 (y 1.08..1.13); the wheel's rim to
// x -.26. In game: CANDIDATE (the game draws its own cab).
// The art is laid out in `width` x `height` design units; on the dash each is faceScale metres (so the
// built-in face is .364 x .221 m, the page art itself unchanged and flat).
inline constexpr float width = .28f, height = .17f;
inline constexpr float faceScale = 1.3f;
inline constexpr float dashFaceZ = -1.28f, dashFaceBottom = .98f, dashFaceTop = 1.25f;
inline constexpr float bezelDepth = .008f; // design units behind the face: the bezel's back sits on the dash
inline constexpr Vec3 mountAt{.04f, 1.115f, dashFaceZ + bezelDepth * faceScale + .002f};
inline constexpr float tabHeight = .028f;
inline constexpr float tabCentre(uint32_t k) { return -width / 2 + tabLeft + (static_cast<float>(k) + .5f) * (width - tabLeft) / pages; }
inline constexpr float pokeDepth = .035f, pokeThrough = .05f, pokeSlack = .012f, pokeGripMost = .3f;
// 0.0.73 test 3 RAY POKE (the screen sits ~0.8-1.0 m from the seat, out of reach without leaning): the free
// hand's aim ray on the face (raySlack design units of margin all round, up to rayReach metres out, not
// grazing: the ray's cosine to the face's normal past rayFacing) plus a fresh trigger press (past rayPress,
// re-armed under rayRelease) pages it as a touch would at that spot. Never a hand on the wheel.
inline constexpr float rayReach = 2.f, raySlack = .03f, rayFacing = .2f, rayPress = .6f, rayRelease = .3f;

// The screen's frame in the van's: unit right (+x) / up (+y) across the face, normal +z (the cab's back).
struct Mount {
    Vec3 centre, right, up, normal;
};
Mount mountLocal();
// Where a van-local point lies on the face, in design units (metres / faceScale): x right, y up, z out of it.
Vec3 onFace(Vec3 local);

// One frame's input, van-local hands (van072::Frame::local).
struct In {
    float dt{};
    bool driving{};  // the screen pages only while he drives
    bool x{};        // a fresh X press this frame
    Vec3 hand[2]{};
    bool tracked[2]{};
    float grip[2]{};
    bool onWheel[2]{}; // van072::Wheel::latched
    // 0.0.73 test 3 RAY POKE: each hand's aim (van-local unit direction; zero: none) and trigger (0-1)
    Vec3 aim[2]{};
    float trigger[2]{};
    bool ray{true}; // false: touch pokes and X only (the 0.0.73 test 3 first cut)
    Offer offer{};  // 0.0.74: the MUSIC / HOME buttons there
};
// Where hand `from`'s ray along `dir` (van-local) meets the face, in design units (x, y; z 0), when it does
// within the face (+ raySlack) and rayReach, from in front, not grazing. False otherwise (or non-finite).
bool rayOnFace(Vec3 from, Vec3 dir, Vec3& hit);

class Screen {
public:
    // Next page (on CASES: the next file first). True: it changed.
    bool next();
    bool previousFile();
    bool pick(uint32_t page);
    // X and pokes; true: the page or file changed. Never consumes anything.
    bool step(const In& in);
    void reset();
    uint32_t page() const { return page_; }
    uint32_t file() const { return file_; }
    float flash() const { return flash_; }
    bool hover() const { return hover_; }
    // 0.0.73 test 3 RAY POKE: hand i's ray was on the face last step (its trigger is the screen's then).
    bool aimed(unsigned i) const { return i < 2 && aimed_[i]; }
    // 0.0.74: the button pressed by the last step (none: none), read once.
    Action takeAction() {
        const Action a = action_;
        action_ = Action::none;
        return a;
    }

private:
    uint32_t page_{}, file_{};
    Action action_{};
    Offer offer_{};
    float flash_{};
    bool near_[2]{}, hover_{};
    bool aimed_[2]{}, trigWas_[2]{};
    bool pokeAt(Vec3 f); // a touch at face point f (design units): the tab row, CASES' back third, else next
};

// What the renderer draws: world space (this frame's), every number checked by append.
struct View {
    bool on{};
    Vec3 centre{}, right{1, 0, 0}, up{0, 1, 0};
    float scale{faceScale}; // metres a design unit (.5 .. 2)
    uint32_t page{}, file{};
    float speed{};    // m/s, + forward
    float topSpeed{16.f};
    float heading{};  // radians clockwise from world -z ("N": CANDIDATE - the game's north is not read)
    float rpm{};      // 0 idle .. 1
    float gridX{}, gridZ{}; // the van's world place (the grid's scroll)
    bool driving{};
    float flash{};
    bool hover{};
    // 0.0.74 test 1: the MUSIC page and the HOME page's buttons.
    Offer offer{};
    bool musicAvailable{true}, musicPlaying{}, musicPaused{};
    int musicTracks{}, musicTrack{-1};
    float musicVolume{70.f};  // 0-100
    float musicElapsed{}, musicTotal{};
    std::string musicSong, musicNote;
    const char* homeNote{};   // a line under the HOME buttons (the last hideout's fault, say)
};
// The view for a van at `frame` (world).
View view(const van072::Frame& frame, const Screen& s, float speed, float topSpeed, float rpm, bool driving);
// Compass heading of a frame (radians, 0..2pi, clockwise from -z).
float headingOf(const van072::Frame& frame);

// Appends the screen for an eye at `viewer` (world; nothing from behind its face - the translucent pass
// would show the text through the back). Nothing for !on or anything non-finite; on any fault (a
// throw, a non-finite vertex) `out` is put back as it was. Returns whether it drew.
bool append(std::vector<Vertex>& out, const View& v, Vec3 viewer);

} // namespace spidy::van_screen
