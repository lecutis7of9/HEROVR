#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 33: SHADOW DISTANCE, live. The read-only search for the CPU value behind the
// sun cascades' size, the trial writes that test it against the next frames' boxes, and
// the put-back. Pure rules in shadow_distance.hpp; this is the Windows side. SEH only in
// POD helpers; heavy work on one below-normal thread; the render thread only feeds the
// boxes it sees (try-lock, never waits). Any failure turns only this off, with one line.
#include "shadow_distance.hpp"
#include <cstddef>
#include <cstdint>

namespace spidy::native_shadow_distance {

// ADDRESS FINDER started: begin the search (once a session; again after NOT FOUND or NO EFFECT).
void start();
// The render thread's upload hook should look at copies for sun boxes.
bool watching();
// Whether this copy (from game call site `site`, a module offset or 0) is within budget.
bool takeCopy(uint32_t site);
// The bytes of a mapped upload copy (read under SEH here); boxes found go to the watch.
void noteUpload(const void* mapped, size_t bytes, uint32_t site);
// The slider: 0 the game's value, else car lengths.
void set(int cars);
// shadow_distance::pack(state, reason), the slider's cars, the game's reach and the live one (m).
uint32_t now();
float wantCars();
float gameReach();
float liveReach();
// VR ends: put the game's value back (waits up to ms for the worker).
void stop(uint32_t ms);

} // namespace spidy::native_shadow_distance
