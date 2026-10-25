#pragma once
#include "body_calibration.hpp"
#include "vertex.hpp"
#include <vector>

// Update 36: the comic style's T-pose calibration panel after the 2004
// console game's title screen: a night street seen from above, the
// "BODY CALIBRATION" title in gold, the crouching Spider-Man the player
// chose in a warm gold halo that grows as the steps are met, the steps in bold italic yellow pixel
// type (the current one pulsing, met ones dimmed), the live hint, a slim
// gold bar and "PRESS B TO SKIP". The pictures come from the art atlas
// (calibration_art.hpp); everything that changes is drawn live.
namespace spidy::calibration_panel {
// Appends the panel as `art` vertices for D3D12Renderer::renderViews (each
// vertex's shape.xy its atlas coordinates; drawn in order, blended, without
// depth). `clock`: seconds, any origin, for the pulse. Nothing while the
// calibration is idle or its place is not finite. The controllers' rings
// are not part of it (body_calibration::appendRings).
void append(std::vector<Vertex>& art, const body_calibration::View&, float clock);
// The hero's halo, 0..1, for a view at `clock` seconds: faint with no step
// met, stronger with each met step (breathing with the current one), full
// and steady once done. 0 when idle.
float glow(const body_calibration::View&, float clock);
} // namespace spidy::calibration_panel
