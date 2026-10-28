#pragma once
// Version 0.0.73 VAN FPS (owner t1048u: "fps drops when going faster in the van").
//
// The export of 28 Oct 17:15 (TEST 3) shows the headset loop at 64 frames a second instead of 90 for the
// whole time the Punisher van was out, parked or driving alike: Spidy's overlay took 15 ms a frame
// (3 ms once he played Batman). The van is a 135,000-triangle model the overlay re-lit and wrote out
// on the CPU every frame (hero007 drawTextured: a cross product, a square root and a normalise per
// corner, then a second pass checking every vertex): about 9 ms here for 413,000 vertices.
//
// drawPrepared keeps each big model's model-space normals once (a turn and one scale keep them; the
// light is turned into model space instead), so a frame only places the corners and takes one dot
// product each. Same vertices as before (tests: van_fps), about a fifth of the time. Anything else
// (a stretched placement, a small model, a model with odd numbers) takes the old path unchanged.
#include "spidy/math.hpp"
#include "spidy/mesh_model.hpp"
#include "spidy/vertex.hpp"

#include <cstdint>
#include <vector>

namespace spidy::vanfps {

// Models under this many triangles keep the old path (their cost is small, and nothing is kept for them).
inline constexpr uint32_t preparedFrom = 4000;
// Kept models (the van's body, the hammers in hand ...): a few, reused round robin.
inline constexpr uint32_t preparedKept = 6;

// Whether o + ex x + ey y + ez z is a turn (or mirror) with one scale, in range: then normals turn
// with it and the prepared path is exact.
bool uniformPlace(Vec3 o, Vec3 ex, Vec3 ey, Vec3 ez, float& scale);

// Appends the model's vertices exactly as hero007's textured path would (position, colour lit by
// the corner's normal, shape u v layer 1) and returns true; false (nothing appended) when the model
// or the placement is not one for this path - the caller then draws it the old way.
bool drawPrepared(std::vector<Vertex>& sink, const mesh_model::Mesh& model, Vec3 o, Vec3 ex, Vec3 ey, Vec3 ez);

// On by default; off: every model the old way (the revert, and the tests' reference).
void setEnabled(bool on);
bool enabled();

struct Stats {
    uint64_t drawn{}, prepared{}, declined{};
};
Stats stats();      // this thread's
void forget();      // this thread's kept models (tests)

} // namespace spidy::vanfps
