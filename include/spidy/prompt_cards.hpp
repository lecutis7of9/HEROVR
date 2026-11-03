#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 PROMPT CARDS (story077d.hpp): one card's vertices - a dark plate with a red rim, the game's
// button on top and the VR control that sends it beneath. The mod's own drawing; never the game's picture.
#include "math.hpp"
#include "vertex.hpp"
#include <string_view>
#include <vector>

namespace spidy::prompt_cards {
// `centre` the card's middle, `right` / `up` unit and square, `height` the top line's capital height (metres),
// `alpha` 0..1. Returns the vertices added (0: nothing drawable, bad numbers).
size_t append(std::vector<Vertex>& out, std::string_view game, std::string_view vr, Vec3 centre, Vec3 right, Vec3 up,
              float height, float alpha);
} // namespace spidy::prompt_cards
