#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
// HERO VR 0.0.77 CUTSCENES (test build cut1). live.txt `cutscenes`: how a scene the game plays on its own camera
// (a cutscene, a finisher, a death) is shown in the headset. Gameplay is never touched by any of this: in VR play
// the eyes stay the tracked ones and the game's prompts stay PROMPT CARDS.
//   0 flat: the game's frame on the virtual screen, as every build before 0.0.78 cut14.
//   1 3d: GAME SCREEN 3D. The game renders its camera once per eye (native mode 3: the camera set apart by the
//     stereo math of native_eye_frame.hpp, each lens shifted the other way) and the screen shows the pair. The
//     SM2 port has no hook for the game's HUD panel (SM2_STEREO_HUD 0), so instead of sliding the panel onto the
//     zero plane the subtitle strip of the game's own frame (its words, drawn where the screen's plane is) is
//     copied onto both eyes' images at the same pixels: no disparity, the words sit on the glass. The 200 ms and
//     1 s rules of presentation_gate.hpp stay, with "the strip landed" as the panel's signal.
//   2 immersive (the default since 0.0.78 cut14; a live.txt cutscenes line still wins): Cutscenes: Immersive. On the scene's first frame a level head (position, yaw only) is kept;
//     each frame each tracked eye relative to that head is put on the camera the game uses that frame (native
//     mode 4, composed in the render frame itself), with the headset's lens, shown as a normal stereo view at
//     the tracked eyes. No hands, webs, markers or body; the controllers stay the game's Xbox controller. A frozen
//     clock (a QTE) holds the scene as long as no menu is up; the menu button or a freeze longer than a QTE goes
//     to the screen. The game's subtitles and prompts are copied from its frame onto a see-through panel in front
//     of the head (a dark glass with the frame's bright, colourless pixels - its words and button icons - keyed on).
// Pure and allocation-free; nothing here reads or writes the game. Made by Agent Mulder.
#include "math.hpp"
#include "native_view.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spidy::cutscene077 {

// live.txt cutscenes (SpidyCutscenes 5571).
enum Mode : uint32_t { flat = 0, screen3d = 1, immersive = 2 };
// HERO VR 0.0.78 cut14: the mode a session starts in when live.txt has no cutscenes line (the owner's ask:
// immersive cutscenes on by default). game_xr.cpp cutscenesNow and live_settings.CUTSCENES_DEFAULT say the same.
inline constexpr uint32_t defaultMode = immersive;
inline const char* modeName(uint32_t mode) {
    return mode == screen3d ? "3D" : mode == immersive ? "IMMERSIVE" : "FLAT";
}

// ---- GAME SCREEN 3D ----
// Whether to ask the game for a scene's two screen eyes (presentation_gate.hpp sceneStereoAsked, plus SM2's
// strip): a scene, GAME SCREEN 3D's depth not off, and either the HUD panel's hook or cutscenes = 3d.
inline bool stereoAsked(bool scene, bool depthOn, bool hudPanelHooked, uint32_t mode) {
    return scene && depthOn && (hudPanelHooked || mode == screen3d);
}
// The game frame's subtitle strip (CANDIDATE: where Spider-Man 2 draws its subtitles, low and centred, from its
// 16:9 frame), in the frame's 0..1 coordinates {u0, v0, u1, v1}.
struct Crop {
    float u0{}, v0{}, u1{1}, v1{1};
};
constexpr Crop subtitleStrip{.12f, .78f, .88f, .97f};
constexpr Crop wholeFrame{0, 0, 1, 1};
// The strip's rectangle {x0, y0, x1, y1} in a screen image's pixels (the screen shows the whole image over its
// own aspect, so the same share of it). False under 8 pixels.
inline bool stripRect(unsigned width, unsigned height, const Crop& c, float out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
    if (!width || !height || !(c.u1 > c.u0) || !(c.v1 > c.v0) || c.u0 < 0 || c.v0 < 0 || c.u1 > 1 || c.v1 > 1)
        return false;
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    out[0] = std::floor(c.u0 * w);
    out[1] = std::floor(c.v0 * h);
    out[2] = std::ceil(c.u1 * w);
    out[3] = std::ceil(c.v1 * h);
    return out[2] - out[0] >= 8.f && out[3] - out[1] >= 8.f;
}
// When the strip last went onto a stereo pair: the panel signal of ScenePanel and of showing the pair.
class StripClock {
  public:
    void landed(uint64_t nowMs) {
        landedMs_ = nowMs;
        any_ = true;
    }
    bool recent(uint64_t nowMs, uint64_t holdMs) const {
        return any_ && nowMs >= landedMs_ && nowMs - landedMs_ <= holdMs;
    }
    void reset() {
        *this = {};
    }

  private:
    uint64_t landedMs_{};
    bool any_{};
};

// ---- Cutscenes: Immersive ----
// A freeze longer than this is no QTE: a menu (CANDIDATE: the game's QTEs wait a few seconds at most).
constexpr uint64_t qteFreezeMaxMs = 15000;
// Whether an immersive scene is shown this frame. `scene`: a scene plays (the screen is up, a player, the bridge,
// not gameplay). `clockRuns`: SceneClock. `menuPressed`: the controllers' menu button is down this frame. A scene
// starts only while its clock runs; once started a frozen clock (a QTE) holds it, unless the menu button was down
// within menuReachMs of the freeze or during it (a pause: the screen) or the freeze outlasts qteFreezeMaxMs; then
// the screen until the clock runs again, which starts the scene anew (started(): a new level head).
constexpr uint64_t menuReachMs = 1000;
class ImmersiveHold {
  public:
    bool update(bool scene, bool clockRuns, bool menuPressed, uint64_t nowMs) {
        started_ = false;
        if (menuPressed) {
            menuMs_ = nowMs;
            menuSeen_ = true;
        }
        if (!scene) {
            on_ = menu_ = false;
            frozenMs_ = 0;
            return false;
        }
        if (clockRuns) {
            frozenMs_ = 0;
            menu_ = false;
            if (!on_)
                started_ = on_ = true;
            return true;
        }
        if (!on_)
            return false;
        if (!frozenMs_ || nowMs < frozenMs_)
            frozenMs_ = nowMs ? nowMs : 1;
        if ((menuSeen_ && nowMs >= menuMs_ && nowMs - menuMs_ <= menuReachMs) || menuPressed ||
            nowMs - frozenMs_ > qteFreezeMaxMs)
            menu_ = true;
        if (menu_) {
            on_ = false;
            ++menus_;
            return false;
        }
        return true;
    }
    // The update that began a stretch: keep a level head now.
    bool started() const {
        return started_;
    }
    // Held through a frozen clock (a QTE) this frame.
    bool frozen() const {
        return on_ && frozenMs_ != 0;
    }
    // The last stretch ended on a menu (or an over-long freeze).
    bool menu() const {
        return menu_;
    }
    // How many stretches ended so.
    uint64_t menus() const {
        return menus_;
    }
    void reset() {
        *this = {};
    }
    // HERO VR 0.0.77 CUTFIX3: a scene's end (any path) lets go of everything it held: the stretch, its freeze
    // (a QTE) and a menu press kept from it; the count stays.
    void release() {
        on_ = menu_ = started_ = menuSeen_ = false;
        frozenMs_ = menuMs_ = 0;
    }

  private:
    uint64_t frozenMs_{}, menuMs_{}, menus_{};
    bool on_{}, menu_{}, started_{}, menuSeen_{};
};
// The level head kept at a scene's first frame: the head's position and only its yaw.
inline Pose levelHead(const Pose& head) {
    const Vec3 forward = head.orientation.rotate({0, 0, -1}), up = head.orientation.rotate({0, 1, 0});
    Vec3 heading{forward.x, 0, forward.z};
    if (length(heading) < .1f)
        heading = forward.y < 0 ? Vec3{up.x, 0, up.z} : Vec3{-up.x, 0, -up.z};
    heading = normalized(heading);
    if (!(length(heading) > .5f))
        heading = {0, 0, -1};
    return {head.position, Quat::yaw(std::atan2(-heading.x, -heading.z))};
}
// A tracked eye relative to the level head (OpenXR axes: x right, y up, z back).
inline Pose relativeEye(const Pose& level, const Pose& eye) {
    return compose(inverse(level), eye);
}
// That relative pose as the renderer's matrix about a camera at the origin (rows right, down, forward, position).
inline Mat4 relativeMatrix(const Pose& relative) {
    const Mat4 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    return native_view::relativePose(identity, relative);
}
// Native mode 4: a relative matrix put on this frame's camera (row vectors: world = relative x camera).
inline Mat4 onCamera(const Mat4& relative, const Mat4& camera) {
    Mat4 out{};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            float s = 0;
            for (int k = 0; k < 4; ++k)
                s += relative[r * 4 + k] * camera[k * 4 + c];
            out[r * 4 + c] = s;
        }
    return out;
}
// A relative eye the native side takes (mode 4): a rotation and within 3 m of the level head.
inline bool relativeUsable(const Mat4& m) {
    return native_view::validPose(m) && std::abs(m[12]) <= 3 && std::abs(m[13]) <= 3 && std::abs(m[14]) <= 3;
}
// The see-through panel: head-locked, `distance` metres ahead, its centre `centreY` (tangent, up positive) and its
// width `width` (tangent) as the head sees it. Each eye sees it from its own side, so it has the disparity of
// that distance. The QTE shape is the whole game frame (its prompts are anywhere on it), larger and higher.
struct PanelShape {
    float centreY{}, width{}, distance{}, backdrop{};
};
constexpr PanelShape subtitlePanel{-.34f, .72f, 2.f, .38f};
constexpr PanelShape promptPanel{-.12f, .86f, 2.f, .22f};
// HERO VR 0.0.77 CUT7 (the owner on cut6: "remove the text line on immersive cam"): the immersive scene has no words
// panel any more - no subtitle strip, ever. Only while a QTE is live (the scene's clock frozen on its prompt) a small
// card a little below the centre holds the game's whole frame keyed (its button icons are anywhere on it), and a menu
// over the scene keeps its whole frame. Anything else: nothing over the scene.
constexpr PanelShape qteCard{-.26f, .5f, 2.f, .3f};
inline bool cardWanted(bool qteLive, bool menu) {
    return qteLive || menu;
}
// HERO VR 0.0.78 CUT9 NO GHOST SCREEN (the owner, t1866u: "a faint, see-through, barely visible 2D ghost screen"
// during immersive scenes). Two paths composited the game's flat frame in an immersive stretch:
// 1. a frame with no scene pair to show (report 025037: waits 18 / 20 / 23 of 479 / 551 / 1953 immersive frames) fell
//    through to the flat VR screen layer for that one frame - single 2D frames among the immersive ones (a ghost);
// 2. the keyed card (the game's whole frame over a 30% glass) followed any frozen scene clock, so a clock that
//    stalls for a frame or two (not a QTE) put a glass flash of the 2D frame over the scene.
// Now: a frame without a pair in the stretch submits no layer at all (the headset keeps the last immersive image),
// never the screen; only a stretch that has had no pair for noPairScreenMs (the eyes never came: a fault, never a
// look) goes back to the screen. The card needs the freeze held qteSettleMs (a QTE holds for seconds), and the
// whole-frame menu panel is never drawn over the scene (menus go to the VR screen: MenuScreen / ImmersiveHold).
constexpr uint64_t noPairScreenMs = 1500, qteSettleMs = 250;
// A frame of the immersive stretch with no scene pair: true to fall back to the flat VR screen (only after
// noPairScreenMs with no pair shown), false to submit no layer.
inline bool waitShowsScreen(uint64_t nowMs, uint64_t lastPairMs, uint64_t waitFromMs) {
    const uint64_t since = lastPairMs > waitFromMs ? lastPairMs : waitFromMs;
    return since && nowMs >= since && nowMs - since >= noPairScreenMs;
}
class QteCard {
  public:
    // frozen: ImmersiveHold::frozen() for the pair shown; true once the freeze has held qteSettleMs.
    bool update(bool frozen, uint64_t nowMs) {
        if (!frozen) {
            fromMs_ = 0;
            return false;
        }
        if (!fromMs_ || nowMs < fromMs_)
            fromMs_ = nowMs ? nowMs : 1;
        return nowMs - fromMs_ >= qteSettleMs;
    }

  private:
    uint64_t fromMs_{};
};
// HERO VR 0.0.77 CUTFIX3 MENU PANEL: a menu over the immersive world (the title screen's, its save slots, CONTINUE,
// its dialogs) is the game's whole frame as it is: not keyed, no glass, full opacity, centred, the size the VR
// screen has (its width at its distance: vr_settings::screenWidth, screenDistance).
inline PanelShape menuPanel(float screenWidth, float screenDistance) {
    const float d = std::isfinite(screenDistance) && screenDistance > .5f ? screenDistance : 2.5f;
    const float w = std::isfinite(screenWidth) && screenWidth > .1f ? screenWidth : 2.f;
    return {0.f, w / d, d, 0.f};
}
enum PanelMode : uint32_t { panelSubtitles = 0, panelPrompts = 1, panelMenu = 2 };
inline const char* panelModeName(uint32_t m) {
    return m == panelMenu ? "menu" : m == panelPrompts ? "prompts" : "subtitles";
}
// Menu mode (CANDIDATE signals): no player record (the title screen: SUIT PROBE "no player yet" there), or the
// player steering a menu in the stretch (D-pad, or the left stick pushed past half, as menus are) - held to the
// stretch's end, as the menu is.
class MenuPanel {
  public:
    uint32_t update(bool shown, bool noPlayer, bool frozen, bool steering) {
        if (!shown) {
            steered_ = false;
            return panelSubtitles;
        }
        steered_ = steered_ || (steering && !frozen);
        if (noPlayer || steered_)
            return panelMenu;
        return frozen ? panelPrompts : panelSubtitles;
    }
    void release() {
        steered_ = false;
    }

  private:
    bool steered_{};
};
// HERO VR 0.0.77 CUT5 MENU SCREEN (the owner on cut4: "the main menu fix just put a 2d flatscreen in the immersive
// not just the option boxes"). A menu over a scene is no longer copied whole onto the immersive world: the stretch
// goes to the normal VR screen, as cutscenes = flat shows it, until the screen is gone (gameplay) or another player
// record is up (a new game or a save loaded). Signals: the session's first screen (the title screen and its menus:
// no gameplay yet, the first player record seen there is the title's), no player record, or the menu button pressed
// in the stretch (the pause menu). CUT6: never the left stick (cut5 ended two immersive scenes of report 234713 as
// "menu (VR screen)" on a stick push: MENU SCREEN #4/#5 "the left stick steering it"), and only while the game's
// screen is up: in VR gameplay (screen false) it is off and resets. Pure; the record is only compared.
// CUT7 FRONT END (the owner on cut6: "just keep the main menu 2d flat regular from the start because its just too
// weird"; report 002416: after the pause at 00:25:40 ET the quit to the main menu showed the title's moving scene
// immersive for 546 frames - the title's new record ended cut6's menu stretch). The front end - the boot, the
// title and its menus, and the way back to it from the pause menu - is always the normal VR screen; immersive is
// only for the game's own scenes. The front end starts with the session and again when the pause menu (the menu
// button in the stretch or within 3 s before it) leads to no player record (a quit or a load from the pause menu).
// It ends with VR gameplay, or when a second new player record (not the one played before the pause) follows a
// no-record gap of at least 1.5 s (the title's record, then the loading screen, then the loaded game's record).
// A pause menu stays on the screen to the end of its stretch. Pure; the record is only compared.
class MenuScreen {
  public:
    // True while the stretch stays on the VR screen. nowMs: a clock in ms (0: 11 ms a call).
    // playing: the screen is down for VR gameplay (a hero played in VR), not for a boot frame with no headset view
    // or no hero yet. HERO VR 0.0.78 CUT9 TITLE FLAT (the owner on cut8: the main menu still starts immersive until
    // the menu button; report 025037 logged MENU SCREEN #1 as "no player record", not the front end, then the title's
    // record went immersive): a boot frame with the screen down ended the front end before the title came up. Only VR
    // gameplay ends it now (and a game loaded from the title, as before).
    bool update(bool screen, uint64_t record, bool menuPressed, uint64_t nowMs = 0, bool playing = true) {
        now_ = nowMs ? nowMs : now_ + 11;
        if (menuPressed)
            pressMs_ = now_ ? now_ : 1;
        if (!screen) {
            if (playing) {
                frontEnd_ = paused_ = false;
                played_ = record;
                seen_ = 0;
            }
            inStretch_ = on_ = false;
            return false;
        }
        if (!inStretch_) {
            inStretch_ = true;
            paused_ = pressMs_ && now_ - pressMs_ <= 3000;
        }
        paused_ |= menuPressed;
        if (paused_ && record == 0 && !frontEnd_) {
            frontEnd_ = true;
            seen_ = 0;
            gapFrom_ = now_;
        }
        if (frontEnd_) {
            if (record == 0) {
                if (last_ != 0 || !gapFrom_)
                    gapFrom_ = now_;
            } else if (record != played_ && record != first_) {
                if (seen_ == 0) {
                    first_ = record; // the title's record
                    seen_ = 1;
                } else if (last_ == 0 && now_ - gapFrom_ >= 1500) {
                    frontEnd_ = paused_ = false; // a game loaded from the title
                    first_ = 0;
                    seen_ = 0;
                }
            }
        }
        last_ = record;
        const bool on = frontEnd_ || paused_ || record == 0;
        if (on && !on_) {
            ++count_;
            why_ = frontEnd_ ? 1 : paused_ ? 2 : 3;
        }
        on_ = on;
        return on_;
    }
    uint64_t count() const {
        return count_;
    }
    bool frontEnd() const {
        return frontEnd_;
    }
    // Why the last stretch went to the screen: 1 the front end, 2 the pause menu, 3 no player record.
    int why() const {
        return why_;
    }
    static const char* whyName(int why) {
        return why == 1 ? "the front end (boot, title, its menus)" : why == 2 ? "the menu button (pause)" : "no player record";
    }

  private:
    uint64_t now_{}, pressMs_{}, gapFrom_{}, played_{}, first_{}, last_{}, count_{};
    int seen_{}, why_{};
    bool frontEnd_{true}, paused_{}, inStretch_{}, on_{};
};

// HERO VR 0.0.78 cut14: game_xr.cpp's two per-frame cutscene decisions, moved here unchanged so tests/cut14_tests.cpp
// replays the player's logs through the code the mod runs (immersive by default must keep the title, its menus, the
// pause menu and pre-rendered videos on the normal VR screen, cut9's TITLE FLAT).
struct FrameInput {
    uint32_t mode{defaultMode};  // cutscenesNow this frame (after the 3d strip fault)
    bool screen{};               // the game's screen is up (no VR gameplay view)
    bool viewing{}, vrPlay{}, gameplay{};
    bool menuPressed{};          // the controller's menu button this frame
    bool heroOk{};               // cutHero.present: a player (or a scene riding a short record gap)
    bool bridge{}, flatScreen{}, head{}, eyesLost{}, fault{};
    bool timeRuns{};             // the scene's clock ran (a pre-rendered video or a pause: the camera is still)
    uint64_t record{}, nowMs{};
};
// The frame is the normal VR screen for a menu (the front end, a pause, no player record).
inline bool menuScreenFrame(MenuScreen& m, const FrameInput& f) {
    return m.update(f.screen && f.mode == immersive, f.record, f.menuPressed, f.nowMs, f.viewing && f.vrPlay && f.gameplay);
}
// The frame is shown immersive (ImmersiveHold: a frozen clock holds a started scene, a menu press ends it).
inline bool immersiveFrame(ImmersiveHold& h, bool menuScreen, const FrameInput& f) {
    return h.update(f.mode == immersive && !f.fault && f.screen && f.heroOk && !menuScreen && f.bridge && !f.gameplay &&
                        !f.flatScreen && f.head && !f.eyesLost,
                    f.timeRuns, f.menuPressed, f.nowMs);
}
// The panel's rectangle in one eye's pixels (y down) from that eye's tangents (left and down negative). eye 0 is
// the left eye; eyeDist the two eyes' distance (metres); aspect the copied part's own width over height.
inline bool panelRect(unsigned eye, float eyeDist, float tanLeft, float tanRight, float tanDown, float tanUp,
                      unsigned width, unsigned height, float aspect, const PanelShape& s, float out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
    if (eye > 1 || !width || !height || !std::isfinite(aspect) || aspect < .2f || aspect > 20.f ||
        !std::isfinite(tanLeft) || !std::isfinite(tanRight) || !std::isfinite(tanUp) || !std::isfinite(tanDown) ||
        tanRight - tanLeft < .1f || tanUp - tanDown < .1f || !std::isfinite(eyeDist) || !(s.distance > .2f))
        return false;
    const float d = std::clamp(eyeDist, .05f, .08f);
    // The left eye is half the distance to the left: the panel's centre is that much to its right.
    const float cx = (eye == 0 ? .5f : -.5f) * d / s.distance;
    const float half = s.width * .5f, halfH = s.width / aspect * .5f;
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const float x0 = (cx - half - tanLeft) / (tanRight - tanLeft) * w;
    const float x1 = (cx + half - tanLeft) / (tanRight - tanLeft) * w;
    const float y0 = (tanUp - (s.centreY + halfH)) / (tanUp - tanDown) * h;
    const float y1 = (tanUp - (s.centreY - halfH)) / (tanUp - tanDown) * h;
    out[0] = std::clamp(x0, 0.f, w);
    out[1] = std::clamp(y0, 0.f, h);
    out[2] = std::clamp(x1, 0.f, w);
    out[3] = std::clamp(y1, 0.f, h);
    return out[2] - out[0] >= 8.f && out[3] - out[1] >= 8.f;
}
// The aspect of a crop of an image of `imageAspect`.
inline float cropAspect(const Crop& c, float imageAspect) {
    const float h = c.v1 - c.v0;
    return h > 0 ? (c.u1 - c.u0) / h * imageAspect : 0.f;
}
// The panel's key (the shader does the same): how much of a game pixel shows on the glass, from its display
// values. Bright and colourless (white words, white button icons) shows; the scene behind them does not.
inline float keyOf(float r, float g, float b) {
    const auto step = [](float a, float b2, float x) {
        const float t = std::clamp((x - a) / (b2 - a), 0.f, 1.f);
        return t * t * (3 - 2 * t);
    };
    const float lum = .2126f * r + .7152f * g + .0722f * b;
    const float sat = std::max({r, g, b}) - std::min({r, g, b});
    return step(.62f, .82f, lum) * (1 - step(.18f, .35f, sat));
}
} // namespace spidy::cutscene077
