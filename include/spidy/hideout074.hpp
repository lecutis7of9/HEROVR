#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder.
//
// Version 0.0.74 test 1 HIDEOUTS: from the van's dash (HOME: GO TO SAFEHOUSE / GO TO BATCAVE) the player is put in
// a room the mod draws itself in VR (the game's picture hidden behind a black box round it), walks it with the
// left stick (snap turns on the right), and leaves by an exit point ("Get in van", "Get in Batmobile", "Return
// to city"): back in the van's seat where he left it. The room is a drop-in model (Models\Hideout\<NAME>\scene.glb,
// drawn unlit: its lighting is baked) and its room.txt (tools/hideout074/make_hideout.py: the spawn, the points,
// the walk grid from the collision model). Pure: no game reads, no addresses. Fail-safe: a room that does not
// read, a non-finite step or a fault anywhere and he is left in (or put back in) the van - never a crash, never
// the flat screen.
#include "spidy/math.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/vertex.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace spidy::hideout074 {

// The wrist option's one-sentence help (combat38 HIDEOUTS; ON is value 0, the default).
inline constexpr const char* help = "Adds safehouse and Batcave trips to the van dash, turning on needs a restart.";

enum class Which : uint8_t { none, safehouse, batcave };
inline constexpr const char* names[3] = {"", "SAFEHOUSE", "BATCAVE"};
// Under Models\ (mesh_model slots slotHideoutSafehouse, slotHideoutBatcave).
inline constexpr const char* folders[3] = {"", "Hideout/Safehouse", "Hideout/Batcave"};

// What a point does: back to the vehicle (the exits), or nothing yet (a "coming soon" point).
enum class Action : uint8_t { none, toVehicle, comingSoon };
Action actionOf(const std::string& name); // room.txt's action word

struct Point {
    std::string id;
    Action action{};
    Vec3 at{};       // room (glTF) space
    float radius{};  // metres, on the floor
    std::string prompt;
};

inline constexpr int16_t blocked = -32768; // a grid cell nobody may stand in
inline constexpr float stepUp = .45f;     // the most the floor may rise from one cell to the next

struct Room {
    std::string name;
    Vec3 spawn{};
    float spawnYaw{}; // degrees, about +y from +x counter-clockwise (Blender's), glTF forward (cos, 0, -sin)
    std::vector<Point> points;
    float killY = -3.f;
    float gridX{}, gridZ{}, cell{.25f};
    int cols{}, rows{};
    std::vector<int16_t> floorCm; // rows x cols: the floor's height in cm, or `blocked`
    bool ok() const;
    // The cell's floor under (x, z) (room space); false: off the grid or blocked.
    bool floorAt(float x, float z, float& y) const;
};
// room.txt's text; false (error said) on anything wrong.
bool parseRoom(const std::string& text, Room& out, std::string& error);

// One frame of the visit.
struct In {
    float dt{};
    Vec3 head{};        // the eye's world position (this frame)
    Vec3 headForward{}; // its world forward (any length; flattened)
    float moveX{}, moveY{}; // the left stick (-1..1; y forward)
    float turnX{};      // the right stick's x
    bool press{};       // a fresh A / trigger press: use the point in reach
    bool exit{};        // a fresh B press held long (or the wrist's exit): back to the vehicle
};
struct Out {
    int point = -1;          // the point in reach (its prompt shown), -1 none
    Action act{};            // what this frame's press asked for
    bool respawned{};        // put back at the spawn (lost, fell, non-finite)
    bool leave{};            // back to the vehicle now
};

inline constexpr float walkSpeed = 2.2f;   // m/s at full stick
inline constexpr float snapDegrees = 30.f;
inline constexpr float stickDead = .2f, turnPress = .7f, turnRelease = .35f;

// The room placed in world space: world = R(yaw) room + at.
struct Place {
    float yaw{}; // radians about +y
    Vec3 at{};
    Vec3 world(Vec3 room) const;
    Vec3 room(Vec3 world) const;
    Vec3 worldDir(Vec3 roomDir) const;
};

class Visit {
public:
    // Into `room` with the eye at `head` looking along `forward`, the real floor at world `floorY`.
    bool begin(Which which, const Room& room, Vec3 head, Vec3 forward, float floorY);
    Out step(const In& in);
    void end();
    bool active() const { return which_ != Which::none; }
    Which which() const { return which_; }
    const Place& place() const { return place_; }
    const Room* room() const { return room_; }
    Vec3 roomFeet() const { return feet_; } // the player's feet in room space (the last good)

private:
    Which which_{};
    const Room* room_{};
    Place place_{};
    Vec3 feet_{};
    float floorY_{}, floorShown_{};
    bool turnHeld_{}, pressWas_{};
    void toSpawn(Vec3 head, Vec3 forward);
};

// The room's draw: the mesh (glTF space) placed by `place`, unlit (its baked colours; an untextured
// triangle shaded a little by its face so a flat placeholder still reads), textured as drawTextured
// does (shape = u, v, layer, 1). Into the model sink; false (nothing added) on any fault or over `budget`.
bool appendRoom(std::vector<Vertex>& sink, const mesh_model::Mesh& mesh, const Place& place, size_t budget);
// The black box round the room (opaque, depth-writing: it hides the game's picture), `margin` past the
// mesh's bounds, at most `most` metres from the eye (the overlay's far plane is 200 m).
bool appendBackdrop(std::vector<Vertex>& out, const mesh_model::Mesh& mesh, const Place& place, Vec3 eye, float margin = 2.f,
                    float most = 150.f);
// Without a room model (the safety net): a black box `half` metres round the eye and a grey floor.
void appendFallbackBox(std::vector<Vertex>& out, Vec3 eye, float floorY, float half = 6.f);

} // namespace spidy::hideout074
