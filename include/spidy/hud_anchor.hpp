#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 82.1 HUD ANCHOR (CANDIDATE), after the player's long-standing reports (cut22 UI feedback; "A and B we never
// fixed it"): (A) the game's health / map HUD that should stay in view flies around, (B) world icons, waypoints and
// zip points run off the side of where they belong and follow the head.
// ROOT CAUSE (from prompt_layer.hpp's cut21 cards and their drawing): the game's cohtml HUD is drawn into a flat layer
// from the GAME's camera (the follow camera, which the mod only steers by the head's look, with a lag and some metres
// behind the hero). cut21 cut each lit piece out of that layer and drew it at its flat-screen place in the EYE's own
// tangents (cardRect): so every piece, a world marker or a corner bar alike, was HEAD-LOCKED - it turned with the head
// instead of staying over its target, was off by however far the game camera's look was from the head's, was placed with
// a guessed 72 degree lens, and swam under the runtime's late reprojection (head-locked pictures do); and a piece's
// place came straight from its latest 100 ms bounding box, which jumps as the picture inside it changes.
// FIX: two classes of piece (Mode auto):
//   WORLD (icons, waypoints, zip points, prompts): the piece's place on the layer becomes a RAY from the game camera's
//     pose and lens (the game's own projection run backwards), the piece is anchored at that ray's point `depth` metres
//     out in the game's world, smoothed there, and every frame drawn from each EYE's own view-projection: over its
//     target in both eyes, still when the head turns, correct per-eye parallax.
//   HUD (the pieces at the layer's edges: health, focus, minimap, gadget bar): placed in front of the head at panelDistance
//     metres on a lazily following panel (native_hud::Follow: holds still while the head looks around a little, glides
//     after a bigger turn), their place in the layer smoothed - comfortable, no jitter, no swim.
// Mode head keeps cut21's head-locked cards; mode world anchors every piece. Pure and allocation-free; reads and writes
// nothing of the game.
#include "math.hpp"
#include "prompt_layer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::hud_anchor {
// live.txt hud_anchor (SpidyHudAnchor 5782): 0 auto (the default), 1 world (every piece anchored), 2 head (cut21's cards).
enum Mode : uint32_t { anchorAuto = 0, anchorWorld = 1, anchorHead = 2 };
// live.txt hud_depth (SpidyHudDepth 5783): metres from the game camera where a world piece is anchored on its ray.
inline constexpr uint32_t depthLow = 5, depthHigh = 300, depthDefault = 40;
inline constexpr float panelDistance = 2.0f;        // metres in front of the head for the HUD class
inline constexpr float smoothSeconds = .12f;        // the smoothing's time constant (layer reads come every 100 ms)
inline constexpr uint64_t slotMaxAgeMs = 300;       // a piece not seen in a fresh read for this long is gone
inline constexpr uint64_t cameraMaxAgeMs = 300;     // an older game camera pose gives no world anchors
inline constexpr float matchU = .08f, matchV = .12f; // a piece is the same one when its centre moved less (layer fractions)
// The HUD class: a piece whose centre is in the layer's outer band (the bars and the map sit at its edges; markers that
// the game clamps to the edge of the screen are directions, head-relative by nature). Hysteresis `margin` for a piece
// already classed, so one on the line does not flip.
inline constexpr float bandU = .18f, bandV = .12f, bandMargin = .03f;

struct Lens {
    float halfW{prompt_layer::fieldTan}, halfH{prompt_layer::fieldTan * 9.f / 16.f}; // tangents, symmetric
    bool fromGame{};
};
// The layer's lens: the game camera's own (its tangent spans, from the view descriptor +400..+40c: left < right,
// bottom < top) when they are sane and have the layer's shape, else the fixed 72 degree guess cut21 used.
inline Lens chooseLens(const float game[4], bool gameKnown, uint32_t layerW, uint32_t layerH) {
    Lens lens;
    lens.halfH = prompt_layer::fieldTan * (layerW ? static_cast<float>(layerH) / static_cast<float>(layerW) : 9.f / 16.f);
    if (!gameKnown || !game || !layerW || !layerH)
        return lens;
    const float spanW = game[1] - game[0], spanH = game[3] - game[2];
    if (!std::isfinite(spanW) || !std::isfinite(spanH) || spanW < .5f || spanW > 4.f || spanH < .3f || spanH > 4.f)
        return lens;
    const float want = static_cast<float>(layerW) / static_cast<float>(layerH), have = spanW / spanH;
    if (std::abs(have - want) > .12f * want)
        return lens;
    lens.halfW = spanW * .5f;
    lens.halfH = spanH * .5f;
    lens.fromGame = true;
    return lens;
}
// A native view pose's (rows right, down, forward, position) ray through the tangent point (x right, y up) of its lens.
inline Vec3 rayDirection(const Mat4& cam, float x, float y) {
    const Vec3 right{cam[0], cam[1], cam[2]}, down{cam[4], cam[5], cam[6]}, forward{cam[8], cam[9], cam[10]};
    return normalized(forward + right * x - down * y);
}
inline Vec3 position(const Mat4& cam) {
    return {cam[12], cam[13], cam[14]};
}
// A region's centre (layer fractions) and card size (tangents: its pixels' span in the lens, shrunk and capped like
// prompt_layer::cardTangents so the card stays small).
struct Shape {
    float u{}, v{}, tw{}, th{};
};
inline bool shapeOf(const prompt_layer::Region& r, uint32_t layerW, uint32_t layerH, const Lens& lens, Shape& out) {
    out = {};
    if (!layerW || !layerH || r.x1 <= r.x0 || r.y1 <= r.y0 || r.x1 > layerW || r.y1 > layerH)
        return false;
    const float w = static_cast<float>(layerW), h = static_cast<float>(layerH);
    out.u = static_cast<float>(r.x0 + r.x1) * .5f / w;
    out.v = static_cast<float>(r.y0 + r.y1) * .5f / h;
    float tw = static_cast<float>(r.x1 - r.x0) / w * 2 * lens.halfW * prompt_layer::shrink;
    float th = static_cast<float>(r.y1 - r.y0) / h * 2 * lens.halfH * prompt_layer::shrink;
    const float fit = std::min({1.f, prompt_layer::maxCardTan[0] / tw, prompt_layer::maxCardTan[1] / th});
    out.tw = tw * fit;
    out.th = th * fit;
    return std::isfinite(out.tw + out.th + out.u + out.v) && out.tw > 0 && out.th > 0;
}
// A layer fraction's tangent point in a lens (x right, y up).
inline void tangentAt(const Lens& lens, float u, float v, float& x, float& y) {
    x = (u * 2 - 1) * lens.halfW;
    y = (1 - v * 2) * lens.halfH;
}
inline bool inBand(float u, float v, float marginU, float marginV) {
    return u < bandU + marginU || u > 1 - bandU - marginU || v < bandV + marginV || v > 1 - bandV - marginV;
}

struct Slot {
    bool used{}, hud{};
    uint64_t lastMs{};
    Shape shape{};               // smoothed
    prompt_layer::Region region{}; // the latest read's pixels (the crop)
    Vec3 world{};                // the world class's smoothed anchor
    bool haveWorld{};
};
struct Counts {
    uint32_t reads{}, pieces{}, matched{}, created{}, worldAnchors{}, hudAnchors{}, noCamera{}, dropped{};
};
// The pieces of the layer across reads: each persistent, smoothed, classed.
class Tracker {
public:
    static constexpr uint32_t most = prompt_layer::maxCards;
    // One fresh read of the layer's regions. `cam`: the game camera's pose with the read (nullptr: none, world pieces
    // keep their last anchor and new ones wait). `depth`: metres. `nowMs`, `seconds` since the previous read.
    void read(const prompt_layer::Region* pieces, uint32_t n, uint32_t layerW, uint32_t layerH, const Lens& lens,
              const Mat4* cam, float depth, uint32_t mode, uint64_t nowMs, float seconds) {
        ++c_.reads;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, .001f, .5f) : .1f;
        const float k = 1.f - std::exp(-dt / smoothSeconds);
        std::array<bool, most> taken{};
        for (uint32_t i = 0; i < n && i < most; ++i) {
            Shape s;
            if (!shapeOf(pieces[i], layerW, layerH, lens, s))
                continue;
            ++c_.pieces;
            int best = -1;
            float bestD = 1e9f;
            for (uint32_t j = 0; j < most; ++j) {
                if (!slots_[j].used || taken[j])
                    continue;
                const float du = std::abs(slots_[j].shape.u - s.u) / matchU, dv = std::abs(slots_[j].shape.v - s.v) / matchV;
                if (du < 1.f && dv < 1.f && du + dv < bestD) {
                    bestD = du + dv;
                    best = static_cast<int>(j);
                }
            }
            bool fresh = false;
            if (best < 0) { // a new piece: a free slot, else the oldest
                uint32_t pick = 0;
                bool found = false;
                for (uint32_t j = 0; j < most; ++j)
                    if (!slots_[j].used) {
                        pick = j;
                        found = true;
                        break;
                    }
                if (!found)
                    for (uint32_t j = 0; j < most; ++j)
                        if (slots_[j].lastMs < slots_[pick].lastMs)
                            pick = j;
                slots_[pick] = {};
                slots_[pick].used = true;
                slots_[pick].shape = s;
                best = static_cast<int>(pick);
                fresh = true;
                ++c_.created;
            } else {
                ++c_.matched;
            }
            Slot& slot = slots_[best];
            taken[best] = true;
            slot.lastMs = nowMs;
            slot.region = pieces[i];
            if (!fresh) { // the HUD class is smoothed where it is on the layer; world pieces are smoothed in the world below
                slot.shape.tw += (s.tw - slot.shape.tw) * k;
                slot.shape.th += (s.th - slot.shape.th) * k;
                slot.shape.u += (s.u - slot.shape.u) * k;
                slot.shape.v += (s.v - slot.shape.v) * k;
            }
            slot.hud = classify(mode, s.u, s.v, slot.hud, !fresh);
            if (!slot.hud) {
                if (!cam) {
                    ++c_.noCamera;
                    continue;
                }
                float x{}, y{};
                tangentAt(lens, s.u, s.v, x, y); // the RAW place: a world icon moves on the layer as the camera turns
                const Vec3 target = position(*cam) + rayDirection(*cam, x, y) * depth;
                if (!finite(target))
                    continue;
                if (!slot.haveWorld || fresh)
                    slot.world = target;
                else
                    slot.world += (target - slot.world) * k;
                slot.haveWorld = true;
            }
        }
        for (auto& slot : slots_)
            if (slot.used && nowMs > slot.lastMs + slotMaxAgeMs) {
                slot = {};
                ++c_.dropped;
            }
    }
    // The slots alive at `nowMs`.
    const std::array<Slot, most>& slots() const {
        return slots_;
    }
    void clear() {
        slots_ = {};
    }
    const Counts& counts() const {
        return c_;
    }
    static bool classify(uint32_t mode, float u, float v, bool wasHud, bool known) {
        if (mode == anchorHead)
            return true;
        if (mode == anchorWorld)
            return false;
        if (known) // a classed piece leaves its class only past the margin
            return wasHud ? inBand(u, v, bandMargin, bandMargin) : inBand(u, v, -bandMargin, -bandMargin);
        return inBand(u, v, 0.f, 0.f);
    }

private:
    std::array<Slot, most> slots_{};
    Counts c_{};
};

// A card of tangent size (tw, th) centred on the world point `at` in one eye's pixels: the eye's row-major
// view-projection `vp` (clip x, y, w in rows 0, 1, 3), its tangents (left, right, up, down; left and down negative) and
// size. The picture's crop {u0, v0, u1, v1} of the layer follows the piece's region, cut by the view's edge like
// prompt_layer::cardRect does. False when behind the eye, under 8 pixels, or off the view.
inline bool anchoredRect(const Mat4& vp, Vec3 at, float tw, float th, const prompt_layer::Region& r, uint32_t layerW,
                         uint32_t layerH, float tanLeft, float tanRight, float tanUp, float tanDown, unsigned width,
                         unsigned height, float rect[4], float crop[4]) {
    rect[0] = rect[1] = rect[2] = rect[3] = 0;
    crop[0] = crop[1] = 0;
    crop[2] = crop[3] = 1;
    if (!finite(at) || !width || !height || !layerW || !layerH || !(tw > 0) || !(th > 0) || !(tanRight - tanLeft > .1f) ||
        !(tanUp - tanDown > .1f) || r.x1 <= r.x0 || r.y1 <= r.y0 || r.x1 > layerW || r.y1 > layerH)
        return false;
    const float cx = vp[0] * at.x + vp[1] * at.y + vp[2] * at.z + vp[3];
    const float cy = vp[4] * at.x + vp[5] * at.y + vp[6] * at.z + vp[7];
    const float cw = vp[12] * at.x + vp[13] * at.y + vp[14] * at.z + vp[15];
    if (!std::isfinite(cx) || !std::isfinite(cy) || !(cw > .05f))
        return false;
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const float px = (cx / cw + 1.f) * .5f * w, py = (1.f - cy / cw) * .5f * h;
    const float halfX = tw * .5f / (tanRight - tanLeft) * w, halfY = th * .5f / (tanUp - tanDown) * h;
    const float x0 = px - halfX, x1 = px + halfX, y0 = py - halfY, y1 = py + halfY;
    if (!(x1 > 0) || !(y1 > 0) || !(x0 < w) || !(y0 < h))
        return false;
    const float cx0 = std::clamp(x0, 0.f, w), cy0 = std::clamp(y0, 0.f, h);
    const float cx1 = std::clamp(x1, 0.f, w), cy1 = std::clamp(y1, 0.f, h);
    if (cx1 - cx0 < 8.f || cy1 - cy0 < 8.f)
        return false;
    const float u0 = static_cast<float>(r.x0) / layerW, u1 = static_cast<float>(r.x1) / layerW;
    const float v0 = static_cast<float>(r.y0) / layerH, v1 = static_cast<float>(r.y1) / layerH;
    crop[0] = u0 + (cx0 - x0) / (x1 - x0) * (u1 - u0);
    crop[2] = u0 + (cx1 - x0) / (x1 - x0) * (u1 - u0);
    crop[1] = v0 + (cy0 - y0) / (y1 - y0) * (v1 - v0);
    crop[3] = v0 + (cy1 - y0) / (y1 - y0) * (v1 - v0);
    rect[0] = cx0;
    rect[1] = cy0;
    rect[2] = cx1;
    rect[3] = cy1;
    return crop[2] > crop[0] && crop[3] > crop[1];
}
// Where a HUD-class piece sits: `facing` (the lazy panel's orientation, native_hud::Follow's, in the head's world frame:
// x right, y up, -z forward) turns the piece's tangent point (x right, y up, cut21's 72 degree place) into a direction;
// the point is panelDistance metres from `head`.
inline Vec3 panelPoint(Vec3 head, const Quat& facing, float x, float y) {
    return head + facing.rotate(normalized(Vec3{x, y, -1.f})) * panelDistance;
}
} // namespace spidy::hud_anchor
