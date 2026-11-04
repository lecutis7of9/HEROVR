// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 CUTFIX2: the one-line note on the headset's screen naming why a scene is not 3d or immersive
// (cutscene077_gate.hpp noteLine), drawn as eye_swap.hpp's lost-eyes notice is: in the screen image's own
// projection (x across -aspect..aspect, y -1..1), a dark band along the top with the line in amber.
#pragma once

#include "overlay_text.hpp"
#include <string_view>
#include <vector>

namespace spidy::cutscene077_note {

inline constexpr float lineHeight = .06f;
inline constexpr float baseline = .86f;

inline void append(std::vector<Vertex>& text, std::vector<Vertex>& backing, std::string_view line, float aspect) {
    if (line.empty())
        return;
    if (!(aspect > 0))
        aspect = 1;
    const float h = fittedHeight(line, lineHeight, 1.8f * aspect);
    const float width = textWidth(line, h);
    TextStyle style;
    style.height = h;
    style.color = Vec3{1, .85f, .2f};
    appendText(text, line, {-width / 2, baseline, .5f}, {1, 0, 0}, {0, 1, 0}, style);
    const float l = -width / 2 - .04f, r = width / 2 + .04f, b = baseline - .04f, t = baseline + h + .04f, z = .6f;
    const Vec3 corners[6] = {{l, b, z}, {r, b, z}, {r, t, z}, {l, b, z}, {r, t, z}, {l, t, z}};
    for (const auto& c : corners)
        backing.push_back({c, {0, 0, 0}, .7f});
}

} // namespace spidy::cutscene077_note
