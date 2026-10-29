#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// 0.0.73 test 4 IMAGE STYLE, shown as GAME TAA (GRAPHICS > QUALITY under IMAGE and AA, combat38::imageStyle,
// live.txt image_style; live).
// The owner, on test 3: "the aliasing is still just too noticeable and if you smooth it ... it just looks
// smudged and blurry ... whatever SpidyVR does to handle it works and it needs to look as good as that."
//
// What SpidyVR (Spider-Man Remastered) does: each eye goes through the game's own TemporalAa with
// its own Halton (2, 3) lens jitter (clamp(ceil(8 ratio^2), 8, 32) phases, 2 x step / render size),
// and the resolved image goes to the headset as it is: no FXAA, no extra history, no CAS, no mip
// bias. HERO 0.0.73 test 3 instead ran FXAA + EYE AA's colour-only history (88%+) + CAS 140 + mip
// bias +0.5, with ENGINE TAA's jitter NONE - and NONE means the game's TAA never runs
// (exe-confirmed, handler 0x2c2dc60: it returns when the job's jitter is zero), so the game's TAA
// was never actually in the picture.
//
// - SPIDYVR (0, default): ENGINE TAA on by itself as soon as nothing blocks it (the game's AA TAA/DLAA
//   and -EngineTaa, the launcher's default), the jitter OURS whenever live.txt says NONE, and the
//   clean picture (native_gpu::setCleanImage: FXAA, history and CAS stand aside, sliders kept).
//   Until it is on, and if it fails (targets gone, frame rate halved, needs a restart), the picture
//   is IMAGE's (EYE AA: SPIDYVR's sharp history read, or HERO VR CLASSIC), so it is never raw or flat.
// - OFF (1): ENGINE TAA off, EYE AA on: the picture is IMAGE's alone (0.0.73 IMAGE, combat38::image).
// The mip bias is IMAGE's (live_settings.image_mip_bias: 0 under its SPIDYVR, read at the game's start).
#include <cstdint>

#include "spidy/engine_taa.hpp"

namespace spidy::image_style {
enum Style : uint32_t { spidyvr = 0, gameTaaOff = 1, styles = 2 };
inline constexpr uint32_t defaultStyle = spidyvr;
inline bool spidy(uint32_t style) { return style != gameTaaOff; } // anything unknown: the default

// The lens jitter mode used while ENGINE TAA is on: SPIDYVR never leaves the jitter at NONE, which
// switches the game's TemporalAa off (it is OURS, SpidyVR's own); GAME and PIXELS stay as asked.
inline uint32_t jitterMode(uint32_t style, uint32_t asked) {
    return spidy(style) && asked == engine_taa::noJitter ? static_cast<uint32_t>(engine_taa::ours) : asked;
}

// What the controller (game_xr, about once a second) does this time.
struct Step {
    bool tryOn{};     // SpidyEngineTaa(1); on success: EYE AA off, the FPS guard armed (tried(true))
    bool engineOff{}; // SpidyEngineTaa(0)
    bool eyeAaOff{};  // EYE AA off (the game's TAA has the picture)
    bool eyeAaOn{};   // EYE AA on (IMAGE's picture), if it is off and can
    bool clean{};     // native_gpu::setCleanImage
    const char* note{}; // a line for the log (once per change)
};
// Inputs: the style, SpidyEngineTaaNow() (engine_taa::pack), SpidyEngineTaaBlocked() (an
// engine_taa::Reason, none when ENGINE TAA could go on now) and whether EYE AA is on.
class Controller {
public:
    Step update(uint32_t style, uint32_t enginePacked, uint32_t blocked, bool eyeAaOn) {
        namespace et = engine_taa;
        Step s;
        const bool wantSpidy = spidy(style);
        if (!known_ || wantSpidy != spidy_) {
            const bool wasSpidy = known_ && spidy_;
            known_ = true;
            spidy_ = wantSpidy;
            owned_ = gaveUp_ = unavailableGaveUp_ = retryNewEyes_ = sawNoEyes_ = false;
            if (!wantSpidy) {
                s.note = "GAME TAA: OFF (EYE AA, IMAGE's look)";
                if (wasSpidy) { // back to IMAGE's picture
                    s.engineOff = et::stateOf(enginePacked) == et::on;
                    s.eyeAaOn = !eyeAaOn;
                }
                return s;
            }
            s.note = "GAME TAA: SPIDYVR (the game's TAA in each eye, nothing over it)";
        }
        if (!wantSpidy)
            return s; // OFF: the player's own presses rule
        const uint32_t state = et::stateOf(enginePacked), reason = et::reasonOf(enginePacked);
        if (state == et::on) { // on (by us, the player or the auto): SpidyVR's picture
            if (!owned_)
                s.note = "GAME TAA SPIDYVR: ENGINE TAA on, FXAA/history/sharpening stand aside";
            owned_ = true;
            gaveUp_ = false;
            s.eyeAaOff = eyeAaOn;
            s.clean = true;
            return s;
        }
        if (owned_) { // it was on and went off
            owned_ = false;
            lastOff_ = reason;
            if (reason == et::vrEnded || reason == et::noEyes)
                return s; // a new VR start tries again
            gaveUp_ = true;
            // 0.0.73 test 5: test 4's two ENGINE TAA OFFs were "as asked" (reason none: a press) and
            // it never came back. A press or lost targets now hold only while these eyes live: the
            // next eyes (after a menu, cutscene or loading) try again. A halved frame rate holds for
            // the session (it would stutter again); GAME TAA OFF is the switch-off.
            retryNewEyes_ = reason != et::slow;
            sawNoEyes_ = unavailableGaveUp_ = false;
            s.eyeAaOn = !eyeAaOn;
            s.note = reason == et::slow ? "GAME TAA SPIDYVR: ENGINE TAA off (frame rate halved), EYE AA instead"
                         : reason == et::targetsGone ? "GAME TAA SPIDYVR: ENGINE TAA lost its targets, EYE AA instead"
                                                     : "GAME TAA SPIDYVR: ENGINE TAA turned off by a press (ENGINE TAA or EYE AA), EYE AA "
                                                       "until the next menu or cutscene (GAME TAA OFF keeps it off)";
            return s;
        }
        if (gaveUp_) {
            if (blocked == et::noEyes) {
                sawNoEyes_ = sawNoEyes_ || retryNewEyes_;
                return s;
            }
            // 0.0.73 test 5: "unavailable" read once (at the eyes' making, before their targets show)
            // is not forever: once nothing blocks it (both eyes have the TAA targets, so no restart
            // is needed after all), it tries again. A real block never clears, so this never flaps.
            if (blocked == et::none && unavailableGaveUp_) {
                s.note = "GAME TAA SPIDYVR: the block cleared (both eyes have the TAA targets), ENGINE TAA again";
            } else if (blocked == et::none && sawNoEyes_) {
                s.note = "GAME TAA SPIDYVR: new eyes, ENGINE TAA again";
            } else {
                return s;
            }
            gaveUp_ = unavailableGaveUp_ = sawNoEyes_ = retryNewEyes_ = false;
            s.tryOn = true;
            return s;
        }
        if (state == et::unavailable && (et::needsRestart(reason) || reason == et::maskDiffers)) {
            gaveUp_ = true;
            unavailableGaveUp_ = true;
            retryNewEyes_ = sawNoEyes_ = false;
            lastOff_ = reason;
            s.note = reason == et::gameAa
                             ? "GAME TAA SPIDYVR: set the game's Anti-Aliasing to TAA (or DLAA) and restart; EYE AA until then"
                         : reason == et::notPrepared
                             ? "GAME TAA SPIDYVR: launch with ENGINE TAA (-EngineTaa) and restart; EYE AA until then"
                             : "GAME TAA SPIDYVR: this game build differs; EYE AA instead";
            return s;
        }
        s.tryOn = blocked == et::none;
        return s;
    }
    // The result of a Step's tryOn.
    void tried(bool on) { owned_ = owned_ || on; }
    bool owned() const { return owned_; }
    bool gaveUp() const { return gaveUp_; }
    // The engine_taa::Reason of the last off it saw or gave up on (none: a press), for the log.
    uint32_t lastOff() const { return lastOff_; }

private:
    bool known_{}, spidy_{}, owned_{}, gaveUp_{};
    bool unavailableGaveUp_{}, retryNewEyes_{}, sawNoEyes_{};
    uint32_t lastOff_{};
};
} // namespace spidy::image_style
