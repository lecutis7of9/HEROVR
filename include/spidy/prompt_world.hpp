#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.78 cut22 PROMPT WORLD (CANDIDATE). Where the game's own UI pieces (prompt_layer's cards: its prompts,
// waypoint and markers, cut from its UI layer) stand in VR. cut21 drew each at its place in the eye's view every frame,
// so they turned with the head (the owner, pt1963u: "it kinda darts around based off where your head is"), and the
// game's prompts at its view's bottom-left edge sat out of a Quest's view, tiny ("so you never see the press y
// prompt"). Now each card is a place in the room (OpenXR tracking space: the floor the player stands on), projected
// each frame through each eye's own pose and lens, so the headset holds it still while the head looks around:
//  - HUD pieces (the game draws them at a fixed spot of its screen: prompts, objective, health) stand on one BOARD,
//    a level plane boardDistance in front of where the player faced when they came up, boardDrop under the eyes.
//    The board stays put while the head looks around; only a turn of the head past turnDegrees (or a step of
//    moveMetres) held for holdMs glides it back in front (glideSeconds). On the board the game's screen is pulled in
//    toward the middle (pull) and each piece grown (grow, at least minCardTan high), so an edge prompt lands well
//    inside a Quest's view at a readable size; capped (maxCardTan), never the whole layer (prompt_layer's guards).
//  - MARKER pieces (the game moves them across its screen as the view turns: they sit on a thing in the world) stand
//    in the direction the game drew them from the head of the moment that grid was read, markerDistance out, so they
//    do not ride along with the head between the 100 ms grids.
// Which is which (Sorter): a piece is a HUD piece until, over successive grids while the head turned, it moved across
// the layer as a world marker would (and not stayed put as a HUD piece does) - markerVotes such grids.
// Pure and allocation-free: no game reads, no writes.
#include "math.hpp"
#include "prompt_layer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace spidy::prompt_world {
// live.txt prompt_anchor (SpidyPromptAnchor 5647). cut22: 0 room, 1 view. Since cut23 (prompt_split.hpp, whose
// Anchor the DLL reads): 0 split (the default), 1 view (cut21's cards at their place in the view, which turn with the
// head), 2 room (cut22's room board, which the owner's video showed left behind as the hero moves; for comparison).
enum Anchor : uint32_t { anchorRoom = 2, anchorView = 1 };

inline constexpr float boardDistance = 1.4f;   // metres in front of the eyes
inline constexpr float boardDrop = .16f;       // metres under the eyes (about 6.5 degrees down)
inline constexpr float pull = .55f;            // the game's screen pulled toward the board's middle
inline constexpr float grow = 1.5f;            // each HUD piece this much its own size in the game's view
inline constexpr float minCardTan = .075f;     // a card at least this high (tangent: about 4.3 degrees)
inline constexpr float maxCardTan[2] = {.34f, .18f}; // at most this wide, high (about 19 x 10 degrees)
inline constexpr float markerDistance = 2.5f;  // metres out for a marker
inline constexpr float markerScale = 1.f;      // a marker card its own size in the game's view
inline constexpr float turnDegrees = 50.f;     // the head turned this far from the board: it glides back
inline constexpr float moveMetres = .6f;       // the head stepped this far from where the board was put: the same
inline constexpr uint64_t holdMs = 800;        // ... held this long first (a glance is not a turn)
inline constexpr float glideSeconds = .3f;     // 63% of the way per this, while gliding
inline constexpr float settleDegrees = 2.f;
inline constexpr uint64_t freshAfterMs = 3000; // no HUD card this long: the next one puts the board anew
inline constexpr float matchUv = .08f;         // a piece within this (layer fractions) of one in the last grid is it
inline constexpr float minTurnTan = .03f;      // a head turn moves a marker this far across the layer to vote (~1.7 deg)
inline constexpr int markerVotes = 2;          // grids voting marker before a piece is drawn as one
inline constexpr int maxVotes = 4;

inline float wrapRadians(float a) {
    if (!std::isfinite(a))
        return 0;
    return a - 6.2831853f * std::round(a / 6.2831853f);
}
// The heading of an orientation in the room (0: facing -z; positive turning left, as Quat::yaw).
inline float yawOf(const Quat& q) {
    const Vec3 f = q.rotate({0, 0, -1});
    const Vec3 u = q.rotate({0, 1, 0});
    // Looking nearly straight up or down the face's way is lost; the top of the head still points it.
    const float pole = std::clamp((std::abs(f.y) - .9f) / .1f, 0.f, 1.f);
    const float hx = f.x - u.x * f.y * pole, hz = f.z - u.z * f.y * pole;
    if (!(hx * hx + hz * hz > 1e-8f))
        return 0;
    return std::atan2(-hx, -hz);
}
inline Vec3 forwardOf(float yaw) {
    return {-std::sin(yaw), 0, -std::cos(yaw)};
}
inline Vec3 rightOf(float yaw) {
    return {std::cos(yaw), 0, -std::sin(yaw)};
}
// A piece's centre in the game's view, in tangent units (x right, y up), as prompt_layer::cardTangents places it.
inline bool pieceTangent(const prompt_layer::Region& r, uint32_t w, uint32_t h, float& tx, float& ty) {
    tx = ty = 0;
    if (!w || !h || r.x1 <= r.x0 || r.y1 <= r.y0 || r.x1 > w || r.y1 > h)
        return false;
    const float fw = static_cast<float>(w), fh = static_cast<float>(h);
    const float cu = static_cast<float>(r.x0 + r.x1) * .5f / fw, cv = static_cast<float>(r.y0 + r.y1) * .5f / fh;
    tx = (cu * 2 - 1) * prompt_layer::fieldTan;
    ty = (1 - cv * 2) * prompt_layer::fieldTan * fh / fw;
    return true;
}
// A piece's natural size in the game's view, tangent units.
inline void pieceSize(const prompt_layer::Region& r, uint32_t w, uint32_t h, float& tw, float& th) {
    const float fw = static_cast<float>(w ? w : 1), fh = static_cast<float>(h ? h : 1);
    tw = static_cast<float>(r.x1 - r.x0) / fw * 2 * prompt_layer::fieldTan;
    th = static_cast<float>(r.y1 - r.y0) / fh * 2 * prompt_layer::fieldTan * fh / fw;
}
// A HUD card's size on the board, tangent units: grown, at least minCardTan high, at most maxCardTan; shape kept.
inline void boardSize(float tw, float th, float& outW, float& outH) {
    outW = tw * grow;
    outH = th * grow;
    if (!(outW > 0) || !(outH > 0)) {
        outW = outH = 0;
        return;
    }
    if (outH < minCardTan) {
        const float up = minCardTan / outH;
        outW *= up;
        outH *= up;
    }
    const float fit = std::min({1.f, maxCardTan[0] / outW, maxCardTan[1] / outH});
    outW *= fit;
    outH *= fit;
}

// THE BOARD: where the HUD pieces stand in the room.
class Board {
public:
    // One frame with HUD cards to show (`shown`) or none, the head (room pose: its eyes' middle) at `nowMs`,
    // `seconds` since the last frame.
    void update(const Pose& head, uint64_t nowMs, float seconds, bool shown) {
        if (!shown)
            return;
        const float hy = yawOf(head.orientation);
        if (!finite(head.position) || !std::isfinite(hy))
            return;
        if (!placed_ || nowMs < lastShownMs_ || nowMs - lastShownMs_ > freshAfterMs) {
            put(head, hy);
            ++puts_;
        }
        lastShownMs_ = nowMs;
        const float off = std::abs(wrapRadians(hy - yaw_)) * 57.29578f;
        const Vec3 d = head.position - eyes_;
        const float moved = std::sqrt(d.x * d.x + d.z * d.z);
        if (!gliding_) {
            if (off > turnDegrees || moved > moveMetres) {
                if (!outsideMs_)
                    outsideMs_ = nowMs ? nowMs : 1;
                if (nowMs - outsideMs_ >= holdMs) {
                    gliding_ = true;
                    ++glides_;
                }
            } else {
                outsideMs_ = 0;
            }
        }
        if (gliding_) {
            const float step = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .25f) : 0.f;
            const float k = 1 - std::exp(-step / glideSeconds);
            yaw_ = wrapRadians(yaw_ + wrapRadians(hy - yaw_) * k);
            eyes_ += (head.position - eyes_) * k;
            const Vec3 left = head.position - eyes_;
            if (std::abs(wrapRadians(hy - yaw_)) * 57.29578f < settleDegrees && length(left) < .03f) {
                gliding_ = false;
                outsideMs_ = 0;
            }
        }
    }
    void reset() {
        placed_ = gliding_ = false;
        outsideMs_ = 0;
    }
    bool placed() const {
        return placed_;
    }
    bool gliding() const {
        return gliding_;
    }
    float yaw() const {
        return yaw_;
    }
    uint32_t puts() const {
        return puts_;
    }
    uint32_t glides() const {
        return glides_;
    }
    // The room point of a piece at (tx, ty) in the game's view: pulled toward the board's middle.
    Vec3 point(float tx, float ty) const {
        const Vec3 centre = eyes_ + forwardOf(yaw_) * boardDistance + Vec3{0, -boardDrop, 0};
        return centre + rightOf(yaw_) * (tx * pull * boardDistance) + Vec3{0, ty * pull * boardDistance, 0};
    }
    Vec3 eyes() const {
        return eyes_;
    }

private:
    void put(const Pose& head, float hy) {
        yaw_ = hy;
        eyes_ = head.position;
        placed_ = true;
        gliding_ = false;
        outsideMs_ = 0;
    }
    Vec3 eyes_{};
    float yaw_{};
    uint64_t lastShownMs_{}, outsideMs_{};
    uint32_t puts_{}, glides_{};
    bool placed_{}, gliding_{};
};

// Whether a head turn from `before` to `now` would carry a world marker at (tx, ty) (tangents of `before`'s view) to
// (px, py) in `now`'s view; false when it would be behind.
inline bool markerMoved(const Quat& before, const Quat& now, float tx, float ty, float& px, float& py) {
    px = tx;
    py = ty;
    const Vec3 room = before.rotate(normalized(Vec3{tx, ty, -1}));
    const Vec3 v = now.conjugate().rotate(room);
    if (!(v.z < -.1f))
        return false;
    px = v.x / -v.z;
    py = v.y / -v.z;
    return std::isfinite(px) && std::isfinite(py);
}

// THE SORTER: which of a grid's pieces are world markers.
class Sorter {
public:
    struct Track {
        float tx{}, ty{};
        int votes{}; // marker votes minus HUD votes
        bool marker{};
    };
    // A new grid's pieces (`n`, their tangents) with the head's room orientation as it was read.
    void update(const float tx[], const float ty[], uint32_t n, const Quat& head) {
        std::array<Track, prompt_layer::maxCards> next{};
        n = std::min<uint32_t>(n, prompt_layer::maxCards);
        const float match = matchUv * 2 * prompt_layer::fieldTan;
        for (uint32_t i = 0; i < n; ++i) {
            Track t{tx[i], ty[i], 0, false};
            int best = -1;
            float bestD = 1e9f;
            for (uint32_t j = 0; j < count_; ++j) {
                // The last grid's piece where it would be now as a marker, and as a HUD piece (unmoved).
                float px{}, py{};
                const bool ahead = markerMoved(head_, head, last_[j].tx, last_[j].ty, px, py);
                const float dHud = std::hypot(tx[i] - last_[j].tx, ty[i] - last_[j].ty);
                const float dMark = ahead ? std::hypot(tx[i] - px, ty[i] - py) : 1e9f;
                const float d = std::min(dHud, dMark);
                if (d < match && d < bestD) {
                    bestD = d;
                    best = static_cast<int>(j);
                }
            }
            if (best >= 0) {
                const Track& was = last_[best];
                t.votes = was.votes;
                t.marker = was.marker;
                float px{}, py{};
                const bool ahead = markerMoved(head_, head, was.tx, was.ty, px, py);
                const float turn = ahead ? std::hypot(px - was.tx, py - was.ty) : 0.f;
                if (ahead && turn > minTurnTan) {
                    const float dHud = std::hypot(tx[i] - was.tx, ty[i] - was.ty);
                    const float dMark = std::hypot(tx[i] - px, ty[i] - py);
                    t.votes = std::clamp(t.votes + (dMark < dHud ? 1 : -1), -maxVotes, maxVotes);
                    if (t.votes >= markerVotes)
                        t.marker = true;
                    else if (t.votes <= 0)
                        t.marker = false;
                }
            }
            next[i] = t;
        }
        last_ = next;
        count_ = n;
        head_ = head;
        ++grids_;
    }
    bool marker(uint32_t i) const {
        return i < count_ && last_[i].marker;
    }
    uint32_t count() const {
        return count_;
    }
    uint64_t grids() const {
        return grids_;
    }
    const Quat& head() const {
        return head_;
    }
    void reset() {
        count_ = 0;
    }

private:
    std::array<Track, prompt_layer::maxCards> last_{};
    uint32_t count_{};
    Quat head_{};
    uint64_t grids_{};
};

// A marker's room point: the direction (tx, ty) of the head `at` (room pose when the grid was read), markerDistance out.
inline Vec3 markerPoint(const Pose& at, float tx, float ty) {
    return at.position + at.orientation.rotate(normalized(Vec3{tx, ty, -1})) * markerDistance;
}

// One eye's card rectangle {x0, y0, x1, y1} (pixels, y down) and crop {u0, v0, u1, v1} for a card `widthMetres`
// wide, `aspect` (width over height), facing the eye at room point `centre`, through `vp` (the eye's room
// view-projection: math.hpp's projection * viewMatrix, column vectors). A part off the view is cut from the rectangle
// and the crop alike. False when behind the eye, off the view or under 8 pixels.
inline bool cardRect(const Mat4& vp, Vec3 centre, float widthMetres, float aspect, float tanLeft, float tanRight,
                     float tanUp, float tanDown, uint32_t width, uint32_t height, const float cropIn[4], float rect[4],
                     float crop[4]) {
    rect[0] = rect[1] = rect[2] = rect[3] = 0;
    for (int i = 0; i < 4; ++i)
        crop[i] = cropIn[i];
    if (!finite(centre) || !(widthMetres > 0) || !(aspect > .05f) || !(aspect < 20.f) || !width || !height ||
        !(tanRight - tanLeft > .1f) || !(tanUp - tanDown > .1f))
        return false;
    const float cx = vp[0] * centre.x + vp[1] * centre.y + vp[2] * centre.z + vp[3];
    const float cy = vp[4] * centre.x + vp[5] * centre.y + vp[6] * centre.z + vp[7];
    const float cw = vp[12] * centre.x + vp[13] * centre.y + vp[14] * centre.z + vp[15];
    if (!std::isfinite(cx) || !std::isfinite(cy) || !(cw > .05f))
        return false;
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const float px = (cx / cw + 1.f) * .5f * w, py = (1.f - cy / cw) * .5f * h;
    const float halfX = widthMetres * .5f / cw * w / (tanRight - tanLeft);
    const float halfY = widthMetres / aspect * .5f / cw * h / (tanUp - tanDown);
    const float x0 = px - halfX, x1 = px + halfX, y0 = py - halfY, y1 = py + halfY;
    if (!std::isfinite(x0 + x1 + y0 + y1) || x1 <= 0 || y1 <= 0 || x0 >= w || y0 >= h)
        return false;
    const float cx0 = std::clamp(x0, 0.f, w), cy0 = std::clamp(y0, 0.f, h);
    const float cx1 = std::clamp(x1, 0.f, w), cy1 = std::clamp(y1, 0.f, h);
    if (cx1 - cx0 < 8.f || cy1 - cy0 < 8.f)
        return false;
    const float cu = cropIn[2] - cropIn[0], cv = cropIn[3] - cropIn[1];
    crop[0] = cropIn[0] + (cx0 - x0) / (x1 - x0) * cu;
    crop[2] = cropIn[0] + (cx1 - x0) / (x1 - x0) * cu;
    crop[1] = cropIn[1] + (cy0 - y0) / (y1 - y0) * cv;
    crop[3] = cropIn[1] + (cy1 - y0) / (y1 - y0) * cv;
    rect[0] = cx0;
    rect[1] = cy0;
    rect[2] = cx1;
    rect[3] = cy1;
    return crop[2] > crop[0] && crop[3] > crop[1];
}
} // namespace spidy::prompt_world
