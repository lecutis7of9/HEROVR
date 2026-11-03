#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 PROMPT WINDOW and BOSS DAMAGE (both CANDIDATE), after the player's Sandman test (t1767u):
// "no highlight on debris and no button prompts at all", and "let the other heroes damage Sandman their own way".
// PROMPT WINDOW (live.txt prompt_window): the game's own button prompts, QTE cues and throw highlights are drawn
// by its Scaleform HUD into its own presented frame only (CONFIRMED: the SM2 HUD and prompt hooks are not ported -
// the report's prompts invokes 0, hud frames 0), never into the eye images. The window shows a copy of that frame
// (native_gpu's screen copy) head-locked low in the view, at infinity (the same tangents in both eyes).
// BOSS DAMAGE (live.txt boss_damage): Sandman's record (.?AVSandmanBot@@, CONFIRMED in the user's world trace) was
// 269 m from the hero - outside world_trace's 30 m enemy list, so no hero weapon could pick him. While a non-Spider-
// Man hero faces the boss, a point along the look stands in for him in the weapon lists (game_xr).
// Pure and allocation-free; nothing here reads or writes the game.
#include "math.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::story077c {
// BOSS DAMAGE
inline constexpr float bossFacingCos = .819f; // 35 degrees, level
inline constexpr float bossProxyMetres = 6.f; // the stand-in's distance along the look (in every weapon's reach)
inline constexpr float bossStrongScale = 4.f; // boss_damage strong: each blow at the boss x4
inline constexpr float bossClapHp = 60.f;     // HULK's thunder clap at the boss (no knockback)
// Whether the look (forward) faces `to` (the boss from the head), both made level.
inline bool facing(Vec3 to, Vec3 forward) {
    to.y = 0;
    forward.y = 0;
    const float a = length(to), b = length(forward);
    if (!std::isfinite(a) || !std::isfinite(b) || a < .5f || b < .2f)
        return false;
    return dot(to * (1.f / a), forward * (1.f / b)) >= bossFacingCos;
}

// PROMPT WINDOW: 0 on, 1 large, 2 off.
enum PromptMode : uint32_t { promptOn = 0, promptLarge = 1, promptOff = 2 };
struct PromptShape {
    float centreY{}, width{}; // tangents (y up; width across)
};
inline PromptShape promptShape(uint32_t mode) {
    return mode == promptLarge ? PromptShape{-.26f, .95f} : PromptShape{-.36f, .62f};
}
// The window's rectangle {x0, y0, x1, y1} in one eye's pixels (y down) from that eye's tangents (left and down
// negative); false when off, the image or lens is unusable, or it would be under 8 pixels.
inline bool promptRect(uint32_t mode, float tanLeft, float tanRight, float tanUp, float tanDown, unsigned width,
                       unsigned height, float imageAspect, float out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
    if (mode > promptLarge || !width || !height || !std::isfinite(imageAspect) || imageAspect < .2f ||
        imageAspect > 8.f || !std::isfinite(tanLeft) || !std::isfinite(tanRight) || !std::isfinite(tanUp) ||
        !std::isfinite(tanDown) || tanRight - tanLeft < .1f || tanUp - tanDown < .1f)
        return false;
    const PromptShape s = promptShape(mode);
    const float half = s.width * .5f, halfH = s.width / imageAspect * .5f;
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const float x0 = (-half - tanLeft) / (tanRight - tanLeft) * w, x1 = (half - tanLeft) / (tanRight - tanLeft) * w;
    const float y0 = (tanUp - (s.centreY + halfH)) / (tanUp - tanDown) * h;
    const float y1 = (tanUp - (s.centreY - halfH)) / (tanUp - tanDown) * h;
    out[0] = std::clamp(x0, 0.f, w);
    out[1] = std::clamp(y0, 0.f, h);
    out[2] = std::clamp(x1, 0.f, w);
    out[3] = std::clamp(y1, 0.f, h);
    return out[2] - out[0] >= 8.f && out[3] - out[1] >= 8.f;
}
inline const char* promptModeName(uint32_t mode) {
    return mode == promptOn ? "ON" : mode == promptLarge ? "LARGE" : "OFF";
}
} // namespace spidy::story077c
