#pragma once
// 79.7 BOLD WEB LINE MARKER's counters (the report's 'webline_marker'): 79.6's tester never saw the green ring while the
// gestures themselves worked (79.6 report: starts and commits, no word on a drawn marker). Now each hand's marker is
// counted three ways: SET (the loop gave the frame a marker for an active gesture), DRAWN (the eye image appended it),
// and the FIRST FRAME's target (did the aim already meet a valid second surface when the first anchor was laid).
#include <cstdint>

namespace spidy::webline_marker {
inline constexpr uint32_t magic = 0x4b4d4c57; // WLMK
inline constexpr float minPixels = 40.f;       // the disc's least diameter, eye pixels
struct Data {
    uint32_t magic = webline_marker::magic, version = 1, bytes = 104, flags{};
    uint64_t gestures{};                // first anchors laid
    uint64_t set[2]{}, drawn[2]{};      // per hand: frames the loop set a marker, frames the eye images drew it
    uint64_t firstFrames{}, firstValid{}; // gestures whose first frame had a second target; of them, a valid one
    uint64_t validFrames{}, invalidFrames{}, noTargetFrames{}, previewDrawn{};
};
static_assert(sizeof(Data) == 104, "SpidyWeblineMarkerData (tools/hero797_record.py)");
} // namespace spidy::webline_marker
