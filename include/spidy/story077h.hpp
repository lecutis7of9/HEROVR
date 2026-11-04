#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 WRIST MAP (CANDIDATE), after the player (t1820u): "left hand trigger hit to right wrist bring up
// the minimap", and (t1824u-t1828u) the game's HUD minimap IS drawn, turning with him, his icon and the objective
// on it - but in VR it sits outside the eyes' view. So the map is a crop of the game's own presented 2D frame (the
// flat frame its window shows, where the HUD is drawn - native_gpu's screen copy, never the eye images), pinned at
// the minimap's HUD corner (the bottom left, scaled by the frame's height and the game's HUD scale), drawn on the
// GPU (the overlay's inset blit, no CPU readback) in a panel over the right wrist in both eyes.
// The gesture: the left hand at the right wrist (tight: within touchMetres of it) and the left trigger pulled -
// a pull begun there toggles the map and is the map's alone (no web, scan or anything of the game's) until let go.
// Pure and allocation-free; nothing here reads or writes the game.
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::story077h {
// live.txt wrist_map: 0 on (the gesture works, the map starts hidden), 1 off (never).
enum Mode : uint32_t { mapOn = 0, mapOff = 1 };
// live.txt wrist_map_size (the panel's width, cm), wrist_map_at (its centre over the wrist, cm), wrist_map_hud (the
// game's HUD scale, percent, so the crop finds its minimap).
inline constexpr uint32_t sizeLow = 5, sizeHigh = 25, sizeDefault = 10;
inline constexpr uint32_t atLow = 0, atHigh = 20, atDefault = 6;
inline constexpr uint32_t hudLow = 50, hudHigh = 150, hudDefault = 100;

// THE GESTURE
inline constexpr float touchMetres = .09f;  // the left grip within 9 cm of the right wrist
inline constexpr float touchStart = .05f, letGo = .03f, pressed = .6f, released = .3f;
inline constexpr float rearmSeconds = .35f; // no second toggle within this
inline constexpr float maxSpeed = 1.5f;     // m/s: a hand moving faster (a punch, a swing) never toggles

// The right wrist: the wrist hologram's emitter point for the right hand (wrist_hud::emitter's, the back of the
// wrist over the grip), so the gesture and the panel sit where the hologram would on that arm.
inline Vec3 rightWrist(const Pose& rightGrip) {
    return compose(rightGrip, Pose{{.045f, .085f, 0}, {}}).position;
}

class Gesture {
  public:
    struct Input {
        bool play{};        // VR play, no phone, wheel or pad layer, no menu
        bool tracked[2]{};  // both grips
        Vec3 left{}, wrist{}; // the left grip, the right wrist (one space)
        float trigger{};    // the left trigger
        bool busy{};        // the left hand is the hologram's, a gun's, a web's: never a toggle
    };
    struct Diagnostics {
        float distance = -1;          // the left grip from the right wrist now (-1: untracked)
        uint32_t presses{}, toggles{}, outOfReach{}, tooFast{}, blocked{}, rearming{}; // pulls begun, and why not
    };
    // seconds since the last update. Returns true when it toggled.
    bool update(float seconds, const Input& in) {
        const bool both = in.tracked[0] && in.tracked[1] && finite(in.left) && finite(in.wrist);
        d_.distance = both ? length(in.left - in.wrist) : -1.f;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .25f) : 0.f;
        since_ += dt;
        float speed = 0;
        if (both && hadLeft_ && dt > 1e-4f)
            speed = length(in.left - lastLeft_) / dt;
        hadLeft_ = both;
        if (both)
            lastLeft_ = in.left;
        const float t = std::isfinite(in.trigger) ? in.trigger : 0.f;
        bool toggled = false;
        // A pull is judged where it begins (the first touch of the trigger), so none of it - not its first
        // light part either - reaches a web or the game when it is the map's.
        if (!engaged_ && t > touchStart) {
            engaged_ = true;
            const bool near = both && d_.distance <= touchMetres;
            claim_ = false;
            if (!near)
                ++d_.outOfReach;
            else if (!in.play || in.busy)
                ++d_.blocked;
            else if (speed > maxSpeed)
                ++d_.tooFast;
            else
                claim_ = true;
        }
        if (engaged_ && !down_ && t > pressed) {
            down_ = true;
            ++d_.presses;
            if (claim_ && since_ < rearmSeconds)
                ++d_.rearming;
            else if (claim_) {
                shown_ = !shown_;
                since_ = 0;
                ++d_.toggles;
                toggled = true;
            }
        } else if (down_ && t < released) {
            down_ = false;
        }
        if (engaged_ && t <= letGo) {
            engaged_ = down_ = claim_ = false;
        }
        return toggled;
    }
    // The left trigger is the map's (a pull begun at the wrist, to its release).
    bool claims() const {
        return claim_;
    }
    bool shown() const {
        return shown_;
    }
    void hide() {
        shown_ = false;
    }
    const Diagnostics& diagnostics() const {
        return d_;
    }

  private:
    Diagnostics d_{};
    Vec3 lastLeft_{};
    float since_ = 1e9f;
    bool hadLeft_{}, engaged_{}, down_{}, claim_{}, shown_{};
};

// THE CROP: where SM2's HUD minimap is in its presented frame (CANDIDATE: the HUD layout's bottom-left corner at
// 100% HUD scale, by the frame's height, so any resolution; anchored to the frame's left edge, so a wider frame
// keeps it at the edge). Generous: the map with a margin round it.
inline constexpr float cropLeft = .028f;   // from the left edge, frame heights
inline constexpr float cropBottom = .019f; // from the bottom edge
inline constexpr float cropSide = .278f;   // the square's side
struct Crop {
    float u0{}, v0{}, u1{1}, v1{1};
};
// False for a frame too small or odd, or a HUD scale outside hudLow-hudHigh.
inline bool hudCrop(uint32_t width, uint32_t height, uint32_t hudPercent, Crop& out) {
    out = {};
    if (width < 64 || height < 64 || hudPercent < hudLow || hudPercent > hudHigh)
        return false;
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    if (w / h < .9f || w / h > 4.f)
        return false;
    const float k = static_cast<float>(hudPercent) / 100.f;
    const float side = cropSide * h * k, left = cropLeft * h * k, bottom = cropBottom * h * k;
    const float x0 = left, x1 = std::min(left + side, w), y1 = h - bottom, y0 = std::max(y1 - side, 0.f);
    out = {x0 / w, y0 / h, x1 / w, y1 / h};
    return out.u1 > out.u0 && out.v1 > out.v0;
}
// Its aspect in pixels (width over height).
inline float cropAspect(const Crop& c, uint32_t width, uint32_t height) {
    if (!height || !(c.v1 > c.v0))
        return 1.f;
    return (c.u1 - c.u0) * static_cast<float>(width) / ((c.v1 - c.v0) * static_cast<float>(height));
}

// THE PANEL: its centre over the right wrist, `atCm` up the grip's back-of-hand axis (+y) from the wrist point.
inline Vec3 panelCentre(const Pose& rightGrip, uint32_t atCm) {
    const Vec3 up = rightGrip.orientation.rotate({0, 1, 0});
    return rightWrist(rightGrip) + up * (static_cast<float>(std::min(atCm, atHigh)) * .01f);
}
// One eye's pixel rectangle {x0, y0, x1, y1} for a panel `widthMetres` wide (`aspect` width over height) facing
// the eye at `centre`, through the eye's row-major view-projection `vp` (column vectors, as math.hpp), its fov
// tangents and target size. A part off the target is cut off the rectangle and the crop alike (never squashed).
// False when the panel is behind the eye, off the view, or under 8 pixels.
inline bool panelRect(const Mat4& vp, Vec3 centre, float widthMetres, float aspect, float tanLeft, float tanRight,
                      float tanUp, float tanDown, uint32_t width, uint32_t height, Crop& crop, float out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
    if (!finite(centre) || !(widthMetres > 0) || !(aspect > .1f) || !(aspect < 10.f) || !width || !height ||
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
    float x0 = px - halfX, x1 = px + halfX, y0 = py - halfY, y1 = py + halfY;
    if (!(x1 - x0 >= 8.f) || !(y1 - y0 >= 8.f))
        return false;
    const float cu = crop.u1 - crop.u0, cv = crop.v1 - crop.v0;
    const float fx0 = std::clamp((0.f - x0) / (x1 - x0), 0.f, 1.f), fx1 = std::clamp((x1 - w) / (x1 - x0), 0.f, 1.f);
    const float fy0 = std::clamp((0.f - y0) / (y1 - y0), 0.f, 1.f), fy1 = std::clamp((y1 - h) / (y1 - y0), 0.f, 1.f);
    if (fx0 + fx1 >= 1.f || fy0 + fy1 >= 1.f)
        return false;
    crop = {crop.u0 + cu * fx0, crop.v0 + cv * fy0, crop.u1 - cu * fx1, crop.v1 - cv * fy1};
    x0 = std::max(x0, 0.f);
    y0 = std::max(y0, 0.f);
    x1 = std::min(x1, w);
    y1 = std::min(y1, h);
    out[0] = x0;
    out[1] = y0;
    out[2] = x1;
    out[3] = y1;
    return x1 - x0 >= 8.f && y1 - y0 >= 8.f;
}

// "NOT ON SCREEN": why there is no minimap to crop (0: there is one). The pixels are never read back (no CPU
// readback), so an empty crop is known by the game's own state that hides its HUD: no fresh presented frame, a
// cutscene or a menu (the VR screen shown), or no hero in play.
enum Missing : uint32_t { haveMap = 0, noFrame = 1, hudHidden = 2, noCrop = 3 };
inline const char* missingText(uint32_t why) {
    return why == noFrame   ? "NO MINIMAP: NO GAME FRAME"
           : why == hudHidden ? "MINIMAP NOT ON SCREEN"
           : why == noCrop    ? "NO MINIMAP: FRAME TOO ODD"
                              : "";
}
inline const char* missingName(uint32_t why) {
    return why == noFrame ? "no game frame" : why == hudHidden ? "HUD hidden (cutscene, menu, no hero)"
                          : why == noCrop ? "frame too odd" : "drawn";
}
} // namespace spidy::story077h
