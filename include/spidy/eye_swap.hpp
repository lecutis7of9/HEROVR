#pragma once

#include "overlay_text.hpp"
#include <cstdint>
#include <vector>

namespace spidy::eye_swap {

// Update 32.1: a game graphics setting changed mid-session (run32b: Ambient
// Occlusion Off -> SSAO) makes the game rebuild its render targets, the two
// eye images included. Before 32.1 a new eye image ended VR (3501, flat and
// frozen). Now the bridge takes the new image when it is the same kind of
// image as the old one, and otherwise pauses the eyes (lostCode) with a
// message in the headset instead of leaving VR.

// The parts of a D3D12_RESOURCE_DESC that the bridge's copies, AA views and
// staged images depend on (plain values, so this is testable off Windows).
struct Desc {
    uint32_t dimension{};
    uint64_t width{};
    uint32_t height{};
    uint32_t depthOrArraySize{};
    uint32_t mipLevels{};
    uint32_t format{};
    uint32_t sampleCount{};
    uint32_t flags{};
};

// D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE and ..._ALLOW_SIMULTANEOUS_ACCESS:
// the flags the AA pass and the state tracking rely on.
inline constexpr uint32_t relevantFlags = 0x8u | 0x20u;

// The eyes stay paused (no new pairs) with this code when a new eye image
// cannot be taken; a non-fatal state, never VR's end.
inline constexpr uint32_t lostCode = 3503;

// Whether `incoming` can replace `current` with nothing rebuilt but the eye's
// shader view (same size, format, layout and relevant flags).
constexpr bool compatible(const Desc& current, const Desc& incoming) {
    return current.dimension == incoming.dimension && current.width == incoming.width &&
           current.height == incoming.height && current.depthOrArraySize == incoming.depthOrArraySize &&
           current.mipLevels == incoming.mipLevels && current.format == incoming.format &&
           current.sampleCount == incoming.sampleCount &&
           (current.flags & relevantFlags) == (incoming.flags & relevantFlags);
}

// A replaced eye image is kept referenced until the bridge's GPU work that
// was submitted before the swap (up to `submitted`) has finished.
constexpr bool releasable(uint64_t retiredAt, uint64_t completed) {
    return completed != UINT64_MAX && completed >= retiredAt;
}

// The headset's message while the eyes are paused (at most 80 characters per
// line, three lines).
inline constexpr const char* lostLines[3] = {
    "A GRAPHICS SETTING REBUILT THE EYE IMAGES",
    "VR SHOWS THE GAME SCREEN UNTIL A RESTART",
    "RESTART THE GAME TO GET 3D BACK",
};
inline constexpr const char* lostMessage =
    "A graphics setting changed the eye images in a way VR cannot follow: the headset shows the game "
    "screen. Restart the game for VR. (3503)";
inline constexpr const char* swappedMessage =
    "A graphics setting rebuilt the eye images; VR picked up the new ones and carries on";

// The paused eyes' notice, drawn over the game screen's image. Positions are
// clip space with x stretched by the image's aspect (width over height): x
// in -aspect..aspect, y in -1..1, z 0.5; noticeProjection undoes the stretch.
inline Mat4 noticeProjection(float aspect) {
    if (!(aspect > 0))
        aspect = 1;
    return {1 / aspect, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}
inline void appendLostNotice(std::vector<Vertex>& out, float aspect) {
    if (!(aspect > 0))
        aspect = 1;
    for (unsigned line = 0; line < 3; ++line) {
        // At most 90% of the width.
        const float h = fittedHeight(lostLines[line], .09f, 1.8f * aspect);
        const float width = textWidth(lostLines[line], h);
        TextStyle style;
        style.height = h;
        style.color = line == 2 ? Vec3{1, .85f, .2f} : Vec3{1, 1, 1};
        appendText(out, lostLines[line], {-width / 2, .25f - static_cast<float>(line) * .2f, .5f}, {1, 0, 0},
                   {0, 1, 0}, style);
    }
}

// A dark band behind the notice (two triangles, for the translucent list).
inline void appendLostBacking(std::vector<Vertex>& out, float aspect) {
    if (!(aspect > 0))
        aspect = 1;
    const float l = -aspect, r = aspect, b = -.3f, t = .45f, z = .6f;
    const Vec3 corners[6] = {{l, b, z}, {r, b, z}, {r, t, z}, {l, b, z}, {r, t, z}, {l, t, z}};
    for (const auto& c : corners)
        out.push_back({c, {0, 0, 0}, .7f});
}

} // namespace spidy::eye_swap
