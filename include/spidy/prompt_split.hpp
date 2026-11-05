#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.78 cut23 PROMPT SPLIT. Where the game's own UI pieces (prompt_layer's cards, cut from its UI layer)
// stand in VR, split by what they are. The owner's cut22 video and words (pt1963u's follow-up): the health bars,
// ability circles and map-style HUD, world-locked on cut22's room board, were left behind when jumping and falling
// ("flying all over the place"; they "should remain stationary in your view"), and the world icons "kinda run off the
// side of where they are supposed to be".
//
// Why (CONFIRMED in code):
//  - cut22's board was fixed in the ROOM (OpenXR tracking space). The game's world moves through the room as the hero
//    swings, falls and turns, so anything fixed in the room is left behind by the world: a room is the wrong frame
//    for a HUD (it belongs to the view) and for a marker (it belongs to a point of the game's world).
//  - cut21 and cut22 read a piece's place in the layer as if the layer spanned a fixed 72-degree view (prompt_layer
//    fieldTan .72). In immersive VR play the game's own view is put ON THE HEAD (stereo_probe.cpp activeView:
//    native_eyes::headView, the head's lens covering both eyes x1.05, widened by the CULL MARGIN, 10 degrees by
//    default) - about 130 degrees across on a Quest 3, not 72. A marker the game drew 50 degrees right was drawn about
//    20 degrees right: off the side of its target, and further off the further out it was.
// CANDIDATE: that the game projects its HUD markers through that view (the one it renders and culls with on the
// head), so a marker's place in its layer is a direction from the head of that frame through headLens.
//
// So, each lit piece is one of:
//  - VIEW HUD (health, focus and gadget bars, the gadget wheel, generic button prompts, the reticle: the game draws
//    them at a fixed spot of its screen): locked to the view (the head), never in the world. Its screen place is
//    pulled in toward the middle (pull) and the piece grown (grow, at least minCardTan high, at most maxCardTan) so
//    an edge piece - the bottom-left Y prompt - lands well inside a Quest's view at a readable size; hudDrop lower.
//  - WORLD MARKER (waypoints, threat and objective icons, a prompt on a thing in the world): the game moves it across
//    its screen as its view turns or travels. cut23 re-read that screen place through the head that is showing the
//    picture, 6 m out, so a turn of the head after the pixels were drawn dragged the icon across the city and a fall
//    left a far objective on the roof under the headset. cut24 locks the icon to the camera that produced those
//    pixels (lockPoint), in that camera's world, and the frame that shows it only projects that world point. The
//    game's own destination position is not a confirmed read (story077g's POI places are CANDIDATE and off; cut10's
//    record scan was never matched to the diamond), so the hold is markerLockMetres along the capture ray.
// Which is which (Split): a piece tracked over the grids (100 ms) keeps a reference place and the head's orientation
// there. When the head has since turned over turnDegrees, a piece still at its place (within moveUv by its steadiest
// edge, so a health bar that shrinks from one side stays still) is HUD-voted, one that moved marker-voted; a piece
// that moved with the head still (the hero travelled) is marker-voted too. Votes are sticky (hysteresis). A piece
// with no votes yet takes its place's prior: in the layer's outer band (where the game keeps its HUD chrome, and
// which lies outside a Quest's view through the head lens) HUD, inside it a marker.
// Pure and allocation-free: no game reads, no writes.
#include "math.hpp"
#include "prompt_layer.hpp"
#include "recording_fov.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace spidy::prompt_split {
// live.txt prompt_anchor (SpidyPromptAnchor 5647): 0 split (the default since cut23), 1 view (cut21's: every card at
// its place in a 72-degree view, turning with the head), 2 room (cut22's room board; for comparison only).
enum Anchor : uint32_t { anchorSplit = 0, anchorView = 1, anchorRoom = 2 };
enum Kind : uint8_t { kindHud = 0, kindMarker = 1 };

// The head lens's tangents (x right, y UP): left < 0 < right, down < 0 < up.
struct Lens {
    float left{}, right{}, down{}, up{};
};
inline constexpr float lensMargin = 1.05f;      // native_eyes::headView's default margin
// recording_fov::widenedTangent (the game's view lens widening), the same arithmetic.
inline float widened(float t, float degrees) {
    return recording_fov::widenedTangent(t, degrees);
}
// The head of a pair of eyes (room poses): between them, their average orientation.
inline Pose headOf(const Pose& a, const Pose& b) {
    Quat q = b.orientation;
    if (a.orientation.x * q.x + a.orientation.y * q.y + a.orientation.z * q.z + a.orientation.w * q.w < 0)
        q = {-q.x, -q.y, -q.z, -q.w};
    Quat m{a.orientation.x + q.x, a.orientation.y + q.y, a.orientation.z + q.z, a.orientation.w + q.w};
    const float n = std::sqrt(m.x * m.x + m.y * m.y + m.z * m.z + m.w * m.w);
    if (!(n > 1e-6f))
        m = a.orientation;
    else
        m = {m.x / n, m.y / n, m.z / n, m.w / n};
    return {(a.position + b.position) * .5f, m};
}
// The game's own view's lens on the head (stereo_probe activeView, native_eyes::headView then viewLens): the smallest
// lens from `head` holding both eyes' images (`fov` {left, right, down, up} radians each), times lensMargin, widened
// by `cullDegrees`; with RECORDING FOV on (`recordingDegrees`, a window `aspect` wide) recording_fov::covering of
// that, as viewLens. False for an unusable pair.
inline bool headLens(const Pose& head, const Pose eyes[2], const float fov[2][4], float cullDegrees, Lens& out,
                     int recordingDegrees = 0, float aspect = 16.f / 9) {
    Lens l{1e9f, -1e9f, 1e9f, -1e9f};
    const Quat back = head.orientation.conjugate();
    for (int e = 0; e < 2; ++e) {
        for (int i = 0; i < 4; ++i)
            if (!std::isfinite(fov[e][i]) || std::abs(fov[e][i]) > 1.5f)
                return false;
        for (float h : {std::tan(fov[e][0]), std::tan(fov[e][1])})
            for (float v : {std::tan(fov[e][2]), std::tan(fov[e][3])}) {
                const Vec3 d = back.rotate(eyes[e].orientation.rotate(Vec3{h, v, -1}));
                if (!(-d.z > .05f))
                    return false;
                const float x = d.x / -d.z, y = d.y / -d.z;
                l.left = std::min(l.left, x);
                l.right = std::max(l.right, x);
                l.down = std::min(l.down, y);
                l.up = std::max(l.up, y);
            }
    }
    if (!(l.left < 0 && l.right > 0 && l.down < 0 && l.up > 0))
        return false;
    out = {widened(l.left * lensMargin, cullDegrees), widened(l.right * lensMargin, cullDegrees),
           widened(l.down * lensMargin, cullDegrees), widened(l.up * lensMargin, cullDegrees)};
    if (recordingDegrees > 0) {
        const auto c = recording_fov::covering({out.left, out.right, -out.up, -out.down}, recordingDegrees, aspect);
        out = {c.left, c.right, -c.bottom, -c.top};
    }
    return out.left > -100 && out.right < 100 && out.down > -100 && out.up < 100;
}
// A layer place (u right, v down, 0..1) as a head tangent (x right, y up) through `lens`.
inline void layerTangent(const Lens& lens, float u, float v, float& tx, float& ty) {
    tx = lens.left + u * (lens.right - lens.left);
    ty = lens.up - v * (lens.up - lens.down);
}

// VIEW HUD placement (cut22's board numbers, now on the view): the layer's screen as +-1 each way, pulled in by
// `pull` of a 72-degree view (prompt_layer::fieldTan, its height by the layer's shape), hudDrop lower.
inline constexpr float hudMetres = 1.4f;     // the HUD's distance (its parallax: a near, readable panel)
inline constexpr float hudDrop = .115f;      // tangent lower (about 6.5 degrees, cut22's boardDrop at 1.4 m)
inline constexpr float pull = .55f;          // kept: the screen's edge comes this much toward the middle
inline constexpr float grow = 1.5f;
inline constexpr float minCardTan = .075f;   // about 4.3 degrees high at least
inline constexpr float maxCardTan[2] = {.34f, .18f};
// A HUD piece's centre (tangents of the view: x right, y up) and size, from its layer rectangle in pixels.
inline bool hudPlace(const prompt_layer::Region& r, uint32_t w, uint32_t h, float& cx, float& cy, float& tw,
                     float& th) {
    cx = cy = tw = th = 0;
    if (!w || !h || r.x1 <= r.x0 || r.y1 <= r.y0 || r.x1 > w || r.y1 > h)
        return false;
    const float fw = static_cast<float>(w), fh = static_cast<float>(h);
    const float halfW = prompt_layer::fieldTan, halfH = prompt_layer::fieldTan * fh / fw;
    const float su = static_cast<float>(r.x0 + r.x1) * .5f / fw * 2 - 1;
    const float sv = 1 - static_cast<float>(r.y0 + r.y1) * .5f / fh * 2;
    cx = su * halfW * (1 - pull);
    cy = sv * halfH * (1 - pull) - hudDrop;
    tw = static_cast<float>(r.x1 - r.x0) / fw * 2 * halfW * grow;
    th = static_cast<float>(r.y1 - r.y0) / fh * 2 * halfH * grow;
    if (th < minCardTan) {
        const float up = minCardTan / th;
        tw *= up;
        th *= up;
    }
    const float fit = std::min({1.f, maxCardTan[0] / tw, maxCardTan[1] / th});
    tw *= fit;
    th *= fit;
    return std::isfinite(cx + cy + tw + th) && tw > 0 && th > 0;
}
// A head-local point at tangent (tx, ty), `metres` ahead of `head` (the -1 axis scaled by `metres`, so the depth
// along the head's forward is `metres`). HUD pieces still use this on the head that is showing them.
inline Vec3 headPoint(const Pose& head, float tx, float ty, float metres) {
    return head.position + head.orientation.rotate(Vec3{tx, ty, -1} * metres);
}
// The capture camera's ray through a marker's tangent, `metres` deep along that camera's forward, in the same
// space as `source`. False when the pose or the tangent is not a place. Holding this point, and projecting it
// later, does not follow a head that has since turned or moved.
inline bool lockPoint(const Pose& source, float tx, float ty, float metres, Vec3& world) {
    world = {};
    if (!(metres > 0) || !std::isfinite(tx + ty + metres) || !finite(source.position))
        return false;
    world = headPoint(source, tx, ty, metres);
    return finite(world);
}
// A game-world point as a tracking-space point of the frame whose world-from-tracking pose is `worldFromTracking`.
inline Vec3 trackingOfWorld(const Pose& worldFromTracking, Vec3 world) {
    return compose(inverse(worldFromTracking), Pose{world, {}}).position;
}

// WORLD MARKER placement. markerMetres is cut23's (a point 6 m out, which a fall puts on a nearby roof). cut24 holds
// the icon at markerLockMetres along the capture ray: from that camera the whole ray is one pixel, and after the
// head moves an 8 m translation stays within about a degree of a 1173 m objective. Not the game's destination; that
// position is not a confirmed read.
// There is no second distance for the web-zip white circle. Session 184004 has reticles 0, so that circle is the
// game's own UI blob in this same layer, not the mod's aim marker (game_swing::Aim.point is the swing-web hit, and
// it is not drawn when reticles are off). No existing hook has a confirmed world position for the game's LT+RT zip
// point (pointLaunch in swing.hpp is the grounded leap window, not that aim). Putting this 1024 m stand-in on a
// near zip point makes the circle slide. It is not separated here, because the layer does not say which blob it is.
inline constexpr float markerMetres = 6.f;
inline constexpr float markerLockMetres = 1024.f;
// CANDIDATE, not a measured zip distance: while the stick-up zip is on screen, a marker is drawn at the tangent
// the game gave it, this far in front of the head, instead of markerLockMetres. A near zip point at 1024 m slides.
inline constexpr float markerViewMetres = hudMetres;
inline float markerDepth(bool zipOnScreen) {
    return zipOnScreen ? markerViewMetres : markerLockMetres;
}
inline constexpr uint32_t markerPadPixels = 12; // the crop and card grown this much each way, both alike: a marker
                                                // that moved since the 100 ms grid stays whole (the alpha keys it)
// A marker piece's centre and size (head tangents) and its padded crop of the layer {u0, v0, u1, v1}.
inline bool markerPlace(const prompt_layer::Region& r, uint32_t w, uint32_t h, const Lens& lens, float& cx, float& cy,
                        float& tw, float& th, float crop[4]) {
    cx = cy = tw = th = 0;
    if (!w || !h || r.x1 <= r.x0 || r.y1 <= r.y0 || r.x1 > w || r.y1 > h || !(lens.right > lens.left) ||
        !(lens.up > lens.down))
        return false;
    const uint32_t x0 = r.x0 > markerPadPixels ? r.x0 - markerPadPixels : 0;
    const uint32_t y0 = r.y0 > markerPadPixels ? r.y0 - markerPadPixels : 0;
    const uint32_t x1 = std::min(w, r.x1 + markerPadPixels), y1 = std::min(h, r.y1 + markerPadPixels);
    const float fw = static_cast<float>(w), fh = static_cast<float>(h);
    crop[0] = x0 / fw;
    crop[1] = y0 / fh;
    crop[2] = x1 / fw;
    crop[3] = y1 / fh;
    float ax{}, ay{}, bx{}, by{};
    layerTangent(lens, crop[0], crop[1], ax, ay);
    layerTangent(lens, crop[2], crop[3], bx, by);
    cx = (ax + bx) * .5f;
    cy = (ay + by) * .5f;
    tw = bx - ax;
    th = ay - by;
    return std::isfinite(cx + cy + tw + th) && tw > 0 && th > 0;
}

// THE SPLIT: which of a grid's pieces are world markers.
inline constexpr float matchUv = .08f;        // a piece within this (layer fractions) of a tracked one is it
inline constexpr float moveUv = .025f;        // moved: its steadiest edge this far (about 1.5 grid cells at 720p)
inline constexpr float turnDegrees = 6.f;     // the head turned this far since a piece's reference: a vote
inline constexpr float bandU = .2f, bandV = .2f; // the layer's outer band: a HUD prior
inline constexpr int markerAt = 2, hudAt = -1, maxScore = 3;
inline float turnedDegrees(const Quat& a, const Quat& b) {
    const float d = std::abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
    return 2 * std::acos(std::min(1.f, d)) * 57.2957795f;
}
// A piece's place in the layer, fractions {x0, y0, x1, y1}.
struct Box {
    float x0{}, y0{}, x1{}, y1{};
    float cx() const {
        return (x0 + x1) * .5f;
    }
    float cy() const {
        return (y0 + y1) * .5f;
    }
};
inline Box boxOf(const prompt_layer::Region& r, uint32_t w, uint32_t h) {
    const float fw = static_cast<float>(w ? w : 1), fh = static_cast<float>(h ? h : 1);
    return {r.x0 / fw, r.y0 / fh, r.x1 / fw, r.y1 / fh};
}
// How far a piece moved between two boxes, by its steadiest edge each way (a bar that grows or shrinks from one end
// keeps the other: still).
inline float movedBy(const Box& a, const Box& b) {
    const float dx = std::min({std::abs(a.x0 - b.x0), std::abs(a.x1 - b.x1), std::abs(a.cx() - b.cx())});
    const float dy = std::min({std::abs(a.y0 - b.y0), std::abs(a.y1 - b.y1), std::abs(a.cy() - b.cy())});
    return std::hypot(dx, dy);
}
inline Kind prior(const Box& b) {
    const float cx = b.cx(), cy = b.cy();
    return cx < bandU || cx > 1 - bandU || cy < bandV || cy > 1 - bandV ? kindHud : kindMarker;
}
// The command serial stored with the layer pixels, when it is the serial of the frame we still have. Zero, or any
// other frame (including the one on the headset now), is not that camera: 0 means there is nothing to lock from.
inline uint64_t matchedCapture(uint64_t pixelSerial, uint64_t storedSerial) {
    return pixelSerial != 0 && pixelSerial == storedSerial ? storedSerial : 0;
}
// One UI-layer piece as the split has it. `how`: 0 dropped (no capture camera), 1 view-locked, 2 world-locked.
// This is the split's vote, not a read of which blob is the objective diamond.
struct PieceNote {
    Kind kind{kindHud};
    float x0{}, y0{}, x1{}, y1{};
    uint8_t how{};
};
// One bounded line. Stops before `cap`. A trailing "|+" means more pieces did not fit. No world position is written.
inline uint32_t pieceLine(char* out, uint32_t cap, int matched, float turnDeg, uint64_t serial, const PieceNote* notes,
                          uint32_t n) {
    if (!out || cap < 8)
        return 0;
    out[0] = 0;
    const int wrote = std::snprintf(out, cap, "cut24 PIECE match=%d turn=%.1f serial=%llu n=%u", matched ? 1 : 0,
                                    std::isfinite(turnDeg) ? turnDeg : -1.f,
                                    static_cast<unsigned long long>(serial), n);
    if (wrote < 0) {
        out[0] = 0;
        return 0;
    }
    uint32_t used = static_cast<uint32_t>(wrote) < cap ? static_cast<uint32_t>(wrote) : cap - 1;
    out[used] = 0;
    n = notes ? std::min(n, prompt_layer::maxCards) : 0;
    for (uint32_t i = 0; i < n; ++i) {
        const char* how = notes[i].how == 2 ? "lock" : notes[i].how == 1 ? "view" : "drop";
        char bit[72];
        std::snprintf(bit, sizeof bit, " | %u%s %.2f,%.2f-%.2f,%.2f %s", i,
                      notes[i].kind == kindMarker ? "M" : "H", notes[i].x0, notes[i].y0, notes[i].x1, notes[i].y1,
                      how);
        const uint32_t need = static_cast<uint32_t>(std::strlen(bit));
        if (used + need + 1 >= cap) {
            if (used + 4 < cap) {
                std::memcpy(out + used, " |+", 3);
                used += 3;
                out[used] = 0;
            }
            break;
        }
        std::memcpy(out + used, bit, need + 1);
        used += need;
    }
    return used;
}

class Split {
public:
    struct Track {
        Box now{}, ref{}, home{};
        Quat refHead{};
        int score{};  // marker votes minus HUD votes
        Kind kind{kindHud};
        bool voted{}, travelled{};
    };
    // A new grid's pieces (`n`, their layer boxes) with the head's room orientation as it was read.
    void update(const Box boxes[], uint32_t n, const Quat& head) {
        std::array<Track, prompt_layer::maxCards> next{};
        std::array<bool, prompt_layer::maxCards> taken{};
        n = std::min<uint32_t>(n, prompt_layer::maxCards);
        for (uint32_t i = 0; i < n; ++i) {
            const Box& b = boxes[i];
            int best = -1;
            float bestD = matchUv;
            const float bw = b.x1 - b.x0, bh = b.y1 - b.y0;
            for (uint32_t j = 0; j < count_; ++j) {
                if (taken[j])
                    continue;
                const Box& o = last_[j].now;
                const float ow = o.x1 - o.x0, oh = o.y1 - o.y0;
                // About the same piece: a similar size (one way at least: a bar's other way stays), close by.
                const bool alike = (bw < ow * 2 && ow < bw * 2) || (bh < oh * 2 && oh < bh * 2);
                const float d = std::hypot(b.cx() - o.cx(), b.cy() - o.cy());
                if (alike && d < bestD) {
                    bestD = d;
                    best = static_cast<int>(j);
                }
            }
            Track t;
            t.now = b;
            if (best < 0) {
                t.ref = t.home = b;
                t.refHead = head;
                t.kind = prior(b);
                ++fresh_;
            } else {
                taken[static_cast<uint32_t>(best)] = true;
                t = last_[static_cast<uint32_t>(best)];
                t.now = b;
                // cut24 session 162732: the diamond's box travelled from the left edge to mid-screen, then one
                // still sample while the capture head turned voted it HUD and the view lock held it there. A piece
                // that has left its first place is not screen-fixed. Health that only shrinks from one end has not.
                if (!t.travelled && movedBy(t.home, b) > moveUv)
                    t.travelled = true;
                const float moved = movedBy(t.ref, b);
                const float turned = turnedDegrees(t.refHead, head);
                int vote = 0;
                if (moved > moveUv)
                    vote = 1; // it travelled across the screen: with the head or with the world
                else if (turned > turnDegrees && !t.travelled)
                    vote = -1; // still at its first place while the head turned: screen-fixed
                if (vote) {
                    t.score = std::clamp(t.score + vote, -maxScore, maxScore);
                    t.ref = b;
                    t.refHead = head;
                    t.voted = true;
                    if (t.score >= markerAt)
                        t.kind = kindMarker;
                    else if (t.score <= hudAt && !t.travelled)
                        t.kind = kindHud;
                    vote > 0 ? ++markerVotes_ : ++hudVotes_;
                }
            }
            next[i] = t;
        }
        last_ = next;
        count_ = n;
        ++grids_;
    }
    Kind kind(uint32_t i) const {
        return i < count_ ? last_[i].kind : kindHud;
    }
    uint32_t count() const {
        return count_;
    }
    uint64_t grids() const {
        return grids_;
    }
    uint64_t markerVotes() const {
        return markerVotes_;
    }
    uint64_t hudVotes() const {
        return hudVotes_;
    }
    uint64_t fresh() const {
        return fresh_;
    }
    void reset() {
        count_ = 0;
    }

private:
    std::array<Track, prompt_layer::maxCards> last_{};
    uint32_t count_{};
    uint64_t grids_{}, markerVotes_{}, hudVotes_{}, fresh_{};
};
} // namespace spidy::prompt_split
