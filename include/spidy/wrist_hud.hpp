#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 14: the wrist hologram (in the manner of Ghost of Tabor's). A small
// soft light on top of one wrist (the left by default, live.txt hud_hand)
// throws faint rays up to a translucent panel floating over it, turned to
// the eyes: a red frame with cut corners and a spider at its top, and in
// electric blue a wireframe Spider-Man, HEALTH, three gadgets with their
// ammunition, and a round minimap with the player's arrow.
//
// Toggled, not gazed at (the user, October 23): bring the other wrist to it
// - wrist to wrist within `reach` - and pull that hand's trigger. The same
// again turns it off; nothing else does. That pull is the hologram's alone
// until the trigger is let go (no web, no reel, nothing of the game's);
// a trigger pulled anywhere else does what it always did.
//
// Everything the panel shows of the game is a PLACEHOLDER today (the panel
// says DEMO): WristHudData is the one place the game's values go.
//
// Update 15: two pages, flipped with the arrow buttons on a strip under
// the panel: 1 STATUS (the above) and 2 VR SETTINGS - RECALIBRATE (the body's
// T-pose calibration, as the SPIDY VR tab's CALIBRATE BODY asks for it), WEB
// THICKNESS (live.txt web_width) and WEB SPEED (web_speed), each slider with
// its number. The other hand's controller points at the panel or touches it
// (Menu): what it is on lights up, and its trigger presses a button or grabs
// a slider and drags it. A pull that starts on the panel is the panel's
// alone until it is let go, so no web or web ball leaves meanwhile.
//
// Update 17: every page is the panel's one size (18 by 22 cm times
// hud_scale); a page whose content is taller scrolls inside it, clipped to
// the inner area, with a slim scrollbar on the right (the pointing hand's
// stick up and down while it points at the panel, or a trigger drag on the
// scrollbar or on empty panel). The arrow strip never scrolls. A fourth
// page, HOLOGRAM: its height over the wrist, its scale, the text size (the
// pages reflow), a colour theme and a text colour.
//
// Update 19: a fifth page, MUSIC: a song list (a dropdown of the Music
// folder's songs), a visualizer, the song's progress (pointed at and pulled:
// it jumps there), previous / back 10 s / play-pause / stop / forward 10 s /
// next, the music's own volume and the visualizer picked (ORIGINAL, BARS,
// FRACTAL BLOOM, TUNNEL, STARBURST). The panel only shows and asks
// (Menu::Result::music); the playing is music_audio.hpp's, on its own thread.
#include "gadget_wheel.hpp"
#include "music_player.hpp"
#include "holo_roster.hpp"
#include "math.hpp"
#include "vertex.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <limits>
#include <vector>

#include "fps_meter.hpp"

namespace spidy::gesture_session {
struct View;
}

namespace spidy::wrist_hud {
// What the panel shows. SM2TODO: every field below is a placeholder; none
// is read from the game yet. Fill this one struct from the game (health,
// each gadget's ammunition, the map) and set demo to false, and the panel
// drops its DEMO mark.
struct WristHudData {
    bool demo = true; // SM2TODO: false once the values below are the game's
    float health = 1; // 0..1. SM2TODO: the player's health (PLACEHOLDER)
    struct Row {
        Gadget gadget{};
        int ammo{}, capacity{}; // SM2TODO: the gadget's charges (PLACEHOLDER)
    };
    // The names are the game's (gadgetName, the weapons' classes); the counts
    // are not. SM2TODO: the equipped gadgets and their charges.
    std::array<Row, 3> gadgets{{{Gadget::impactWeb, 6, 6}, {Gadget::webBomb, 6, 6}, {Gadget::tripMine, 6, 6}}};
    // The minimap's streets are a decorative grid. SM2TODO: the game's map
    // around the player. Its turn is real: the headset's heading.
};

// Where the wrist joint is from a hand's grip pose (body::Config's
// wristFromGrip, x toward the back of the hand), and the hologram's light:
// on top of that wrist, 2.5 cm further out on the back of the hand.
// hand: 0 the left, 1 the right.
Vec3 wrist(const Pose& grip, unsigned hand);
Vec3 emitter(const Pose& grip, unsigned hand);

// The toggle. Update 20: the other hand anywhere along the hologram arm's
// forearm - its grip or its wrist within `reach` (15 cm) of the segment from
// the hologram hand's grip `forearm` (28 cm) back toward its elbow
// (armDistance) - and its trigger pulled past `pressed` (a rising edge) there
// toggles. Until Update 20 it was the two wrist joints within 11 cm, and the
// pull had to start (past `touch`) already in reach: the October 23 Quest 3
// player "clicked all up and down my left arm" and it never opened (their
// report: the right grip came within 15 cm of the left forearm in 9% of the
// play, the wrist joints within 11 cm in under 1%; controllers that bulky
// keep the grips 10 cm apart at best). A pull begun in reach is claimed from
// its start (hands(): neither a web nor anything of the game's sees it); one
// begun out of reach and pressed in reach is claimed from the press, until
// the trigger is let go (under `rest`). A second toggle needs `rearm`
// seconds after the first. A trigger already held as the hands meet stays
// the game's. The panel fades in and out over fadeSeconds. flip(): the
// fallback (F8 on the PC's keyboard), the same toggle without the gesture.
// diagnostics(): what the gesture saw, for the session's report.
class Toggle {
  public:
    static constexpr float reach = .15f, forearm = .28f, touch = .12f, rest = .05f, pressed = .6f,
                           released = .3f, rearm = .25f, fadeSeconds = .15f;
    struct Diagnostics {
        float distance = -1;      // metres from the hologram forearm now (-1: a hand untracked or not playing)
        float nearest = -1;       // the least distance since the session started (-1: none yet)
        float pressDistance = -1; // at the pointing hand's last pull past `pressed` (-1: none, or untracked)
        uint32_t presses{};       // the pointing hand's pulls past `pressed` while the controllers play VR
        uint32_t pressesInReach{}, pressesOutOfReach{};
        uint32_t pressesNotPlaying{}; // pulls while not playing (phone, wheel, pad layer, a menu, not in the game)
        uint32_t pressesRearming{};   // pulls in reach inside `rearm` of the last toggle
        uint32_t flips{};             // toggles by flip() (the keyboard)
        bool inReach{};
        // Update 38 HOLO OPEN: opens waited for (38's SAFE / LONG PRESS; 0 since 38.1), the
        // opening pulls cancelled (38.1: a CLICK with the hologram hand's trigger pulled, the
        // Sonic Burst, swallowed), and pulls F8 ONLY left to the game.
        uint32_t deferred{}, cancelled{}, leftToGame{};
        // Update 38.1 HOLO ZONE: presses (trigger or grip) begun in the zone and swallowed to
        // their release, taps that toggled, PALM LOOK opens and closes, the pointing hand's
        // distance from the zone now (-1 untracked) and whether the palm-up look holds.
        uint32_t zoneSwallowed{}, taps{}, lookOpens{}, lookCloses{};
        uint32_t forearmClicks{}, forearmFast{}; // Update 38.2 FOREARM: clicks that toggled, held off as too fast
        float zoneDistance = -1;
        bool palmLook{}, inZone{};
    };
    // seconds: since the previous update. play: the controllers play VR (no
    // phone, wheel or pad layer). hudHand: the hologram's wrist (0 left).
    // grips, tracked and triggers: each hand's, in one space. Returns true
    // when it toggled on this update.
    // squeezes: each hand's grip (38.1: the zone swallows it too); null: none pressed.
    bool update(float seconds, bool play, unsigned hudHand, const Pose grips[2], const bool tracked[2],
                const float triggers[2], const float squeezes[2] = nullptr);
    // Update 20: toggles now (the keyboard fallback); false inside `rearm`.
    bool flip();
    // Update 38.1 HOLO OPEN (combat38::HoloOpen), set before each update; never set: the
    // Update 20 toggle (a click in reach, at once), which the 36 / 37.2 tests keep.
    //   0 TAP       the other hand's tip taps the 7 cm zone over the hologram wrist (in it
    //               `tapDwell`, then out, or `tapHold` held) with the palm-up look holding
    //               (palmLook: the palm toward the face, the head looking at the wrist); a
    //               click in the zone with that look is a tap too
    //   1 PALM LOOK the palm-up look held `lookHold` opens it, broken `lookLetGo` closes one
    //               it opened; TAP still works
    //   2 CLICK     Update 20's click in reach, at once, no look needed (a click with the
    //               hologram hand's trigger pulled - the Sonic Burst - never); TAP still works
    //   3 F8 ONLY   no gesture (and no zone: every pull is the game's); F8 still works
    //   4 FOREARM   (Update 38.2, the option's default) a click by the other hand's trigger
    //               with it on the hologram forearm (armDistance within forearmReach) opens or
    //               closes at once - no look, no extended arm; the pointing hand moving faster
    //               than forearmSpeed (a punch, a swing) never; the hologram hand's trigger
    //               pulled (the Sonic Burst) never. Its zone is the forearm's (forearmZone):
    //               the trigger and grip pressed there are swallowed to their release.
    // In 0-2 the ZONE swallows the pointing hand's trigger and grip pressed in it (or within
    // `zoneLinger` of leaving it) until both are let go - never handed back to the game, so
    // no web ball, punch or Sonic Burst; there is no hands-apart guard (Update 38's blast
    // pose cancelled every open, run38). Closing works the same way as opening.
    static constexpr float zone = .07f, zoneForearm = .12f, zoneLinger = .25f, tapDwell = .12f, tapHold = .30f,
                           lookHold = .60f, lookLetGo = 1.0f, palmFacing = .5f, lookDegrees = 25.f;
    static constexpr float forearmReach = .12f, forearmZone = .09f, forearmSpeed = 1.5f;
    // 38's (still the ids' meaning for the 38 tests' log lines).
    static constexpr float openDelay = .20f, longHold = .50f;
    void setMode(uint32_t mode) {
        mode_ = mode <= 4 ? mode : 0u;
    }
    // The head (tracking space, as the grips) for the palm-up look; without one the look
    // never holds (TAP and PALM LOOK cannot open; CLICK and F8 still can).
    void look(const Pose& head, bool valid);
    // The pointing hand's distance from the zone (the nearer of its grip, wrist joint and
    // controller tip to the hologram wrist's forearm segment); <= zone: in it.
    static float zoneDistanceOf(const Pose& point, unsigned pointHand, const Pose& arm, unsigned armHand);
    // The palm-up look: the hologram palm toward the face and the head looking at the wrist.
    static bool palmLookOf(const Pose& arm, unsigned armHand, const Pose& head);
    // A bit per hand whose trigger AND grip the zone swallows (a subset of hands()).
    uint32_t zoneHands() const {
        return zoneHands_;
    }
    bool pending() const {
        return pendingHand_ < 2;
    }
    bool on() const {
        return on_;
    }
    void set(bool on) {
        on_ = on;
    }
    // 0 hidden .. 1 shown, faded.
    float opacity() const {
        return opacity_;
    }
    // A bit per hand whose trigger the toggle holds.
    uint32_t hands() const {
        return hands_ | zoneHands_;
    }
    uint32_t toggles() const {
        return toggles_;
    }
    const Diagnostics& diagnostics() const {
        return diagnostics_;
    }

  private:
    bool on_{}, down_[2]{}, touched_[2]{};
    float opacity_{}, wait_{};
    uint32_t hands_{}, toggles_{};
    // Update 38.1 HOLO OPEN
    uint32_t mode_ = 9, pendingHand_ = 2; // 9: never set (Update 20's, at once)
    Pose lastPoint_{};
    bool lastPointValid_{};
    uint32_t zoneHands_{};
    bool headValid_{}, swallowing_[2]{}, squeezed_[2]{}, tapDone_{}, lookOpened_{}, lookSpent_{};
    Pose head_{};
    float linger_{}, dwell_{}, lookSeconds_{}, brokenSeconds_{};
    Diagnostics diagnostics_{};
};
// Update 20: how far the pointing hand (grip `point`, hand `pointHand`) is
// from the hologram arm's forearm (grip `arm`, hand `armHand`): the nearer of
// its grip and its wrist joint to the segment from the arm's grip `forearm`
// back along the forearm. The forearm is the grip's +y turned 30 degrees
// toward its +z (the little finger's side): a hand closed round a Touch or
// Index controller's handle holds it tilted that far forward of square to
// the forearm, so the grip's own +y rises off the arm (14 cm above it at the
// elbow end) where the player's other hand never is.
float armDistance(const Pose& point, unsigned pointHand, const Pose& arm, unsigned armHand);

// Update 15: the panel's controls, and the values its pages show and set.
enum class Control : uint8_t {
    none,
    previous,
    next,
    recalibrate, // page 2, VR SETTINGS
    thickness,
    speed,
    aa, // page 3, GRAPHICS
    history,
    reach,
    monitor,
    supersample, // page 3, NEXT LAUNCH: restart required
    sharpen,     // page 3, live (Update 16)
    reticles,    // page 2, live (Update 16): the aim reticles on or off
    hudHeight,   // page 4, HOLOGRAM (Update 17): all live
    hudScale,
    hudText,
    hudTheme,
    hudTextColor,
    webStyle,    // page 2, live (Update 18): the webs' look (web_visual.hpp web_style)
    webColor,    // page 2, live: a shade from the style's own list (web_visual.hpp web_style::shadeName)
    // Update 19: page 5, MUSIC. musicTrack is the song dropdown (its bit in
    // a changed mask: the song picked changed, for live.txt music_track);
    // musicPick a row of its open list; musicSeek the progress bar.
    musicTrack,
    musicPrevious,
    musicRewind,
    musicPlay,
    musicStop,
    musicForward,
    musicNext,
    musicVolume,     // live: music_volume, percent
    musicVisualizer, // live: music_visualizer (music::visualizerName)
    musicSeek,
    musicPick,
    // Update 24: page 3, live: EYE TEST, both eyes shown the left eye's picture
    // (live.txt eye_debug, eye_debug.hpp), a flicker test; never kept.
    eyeTest,
    // Update 24: page 2, the session FPS's RESET (FpsSession::reset): a press,
    // never a setting, so it has no bit in a mask (bit() is 0 for it).
    fpsReset,
    // Update 27: page 3 GRAPHICS, ADDRESS FINDER: a press (like fpsReset), never
    // a setting / never kept. Display comes from View::addressFinder*.
    addressFinder,
    // Update 28: page 3 GRAPHICS, EYE LOD: a live press (like addressFinder),
    // default OFF, never kept. Display comes from View::eyeLodStatus.
    eyeLod,
    // Update 31: page 3 GRAPHICS, SHADOW CAMERA: a live press stepping OFF ->
    // STOCK NEAR -> FLAT -> OFF (shadow_camera.hpp), default OFF, never kept.
    // Update 32: retired from the page (no visible effect); the entry stays so indices hold.
    // Display comes from View::shadowCamera.
    shadowCamera,
    // Update 33: page 3 GRAPHICS, SHADOW DISTANCE: a live slider past the mask (0 the
    // game's own, else car lengths), never kept; locked (Input/View::shadowDistance)
    // until the search finds the value (shadow_distance.hpp). Its change is Result::shadowDistance.
    shadowDistance,
    // Update 33: page 3 GRAPHICS, EYE AA: a press (on/off), live, default OFF, never kept;
    // shown from View::eyeAa (eye_taa.hpp pack). Result::eyeAa.
    eyeAa,
    // Update 34: page 3 GRAPHICS, ENGINE TAA: a press (on/off), live, default OFF, never
    // kept; shown from View::engineTaa (engine_taa.hpp pack). Result::engineTaa.
    engineTaa,
    // Update 34.2: page 3 GRAPHICS, EYE AA TRAIL: a live slider past the mask (0-100, the
    // percent of EYE AA's history kept at swing speed; eye_history::movedWeight), never
    // kept here (live.txt trail keeps it). Its change is Result::eyeTrail.
    eyeTrail,
    // Update 34.2: page 3 GRAPHICS, TAA JITTER: a press stepping ENGINE TAA's jitter
    // OURS -> GAME -> PIXELS -> NONE (engine_taa::JitterMode), live, never kept. Shown from
    // View::taaJitter. Result::taaJitter. Update 35: the press steps OURS <-> NONE.
    // 0.0.73 TAA SHAKE: the press steps FULL -> HALF -> OFF (the jitter's scale, OURS; default HALF).
    taaJitter,
    // Update 35: page 3 GRAPHICS, FAR SHADOWS: a press stepping OFF -> EYES -> EYES+CACHE
    // (far_shadows.hpp), live, default EYES, never kept. Shown from View::farShadows
    // (far_shadows::pack). Result::farShadows.
    farShadows,
    // Update 36: page 3 GRAPHICS, EYE BLOOM: a press stepping OFF -> COPY -> GATE
    // (eye_bloom.hpp), live, default OFF, never kept. Shown from View::eyeBloom.
    eyeBloom,
    // Update 36: page 3 GRAPHICS, SHARED LOD: a press stepping OFF -> GAME VIEW -> HIGHER
    // (shared_lod.hpp), live, default OFF, never kept. Shown from View::sharedLod.
    sharedLod,
    // Update 37: page 2 VR SETTINGS, MIRROR: a press showing or dismissing the mirror
    // (vr_mirror.hpp), live; shown from View::mirror (on, and its cost). Result::mirror.
    mirror,
    // Update 37: page 2, MIRROR RECENTRE: a press, the mirror placed again in front of you.
    mirrorRecentre,
    // Update 37: page 2, WORLD LOG: a press turning the read-only NPC / spawn / mission
    // log on or off (world_trace.hpp), live, default ON; shown from View::worldLog.
    worldLog,
    // Update 37: page 2, BLOOD SPLATTER: a press stepping OFF -> ON -> BRUTAL (intensity
    // 0 / 100 / 175, blood_splatter.hpp), live, default OFF; shown from View::blood.
    blood,
    // Update 37.2: page 2, MIRROR ANCHOR: a press stepping PLAYSPACE <-> WORLD (where the
    // mirror stays: travels with you, or pinned in the city); live.txt mirror_anchor.
    mirrorAnchor,
    // Update 37.2: page 2, NPC NEAR FADE: a press stepping GAME -> OFF -> SWITCH (npc_fade.hpp),
    // live, default OFF (people near you are no longer hidden); shown from View::npcFade.
    npcFade,
    // Update 37.2: page 2, SLOT TEST: a press trying the next gadget input 1.5 s later
    // (combat_input::SlotTest), never kept; shown from View::slotTest.
    slotTest,
    // Update 37.2: page 3 GRAPHICS, WEB BALL TEST: a press stepping EYE BLOOM OFF <-> COPY and
    // shooting one web ball 1.5 s later (the gold squares' A/B); shown from View::webBall.
    webBallTest,
    // Update 37.2: page 7 COLOR (eye_color.hpp), all live, never past the session unless
    // live.txt keeps them: COLOR FX (a press, ON/OFF), FILTER (a press, the next preset) and
    // eight sliders. Their changes are Result::colorFx / colorFilter / colorSlider.
    colorFx,
    colorFilter,
    colorStrength,
    exposure,
    contrast,
    saturation,
    vibrance,
    warmth,
    tint,
    vignette,
    // Update 37.2: page 3 GRAPHICS, FOVEATED (EXPERIMENTAL, foveation.hpp): a press stepping
    // OFF -> LIGHT -> BALANCED -> AGGRESSIVE, INNER SIZE (a live slider), FOVEATED DEBUG (a
    // press, the zones tinted) and FOVEATED FPS (this level's HIGH / LOW / AVG; a press resets).
    foveated,
    foveaInner,
    foveaDebug,
    foveaStats,
    // Update 38: page 2, the COMBAT settings (combat38.hpp optionInfo, in its order): each a
    // press stepping to the next value, live (no restart), never past the session unless
    // live.txt keeps it; shown from View::combat38, pressed as Result::combat38.
    combatCrawlCap,
    combatZipMode,
    combatBallAim,
    combatBallLook,
    combatPunchMode,
    combatRingAnchor,
    combatHoloOpen,
    combatWallTilt,
    combatWebRope,
    combatLunge,
    combatBallFire,
    // Update 38.1: page 2, the COMBAT fixes' plan B (combat38.hpp 11-18), each live.
    combatAimAxis,
    combatRingLayer,
    combatLungeFrom,
    combatLungeSpeed,
    combatLungeAim,
    combatPunchAngle,
    combatBallLead,
    combatBloodLook,
    // Update 38.2: MELEE ANIM, GADGET KEYS, GRAB WEB, MIRROR LOOK, GADGET FEEL (combat38.hpp 19-23).
    combatMeleeAnim,
    combatGadgetKeys,
    combatGrabWeb,
    combatMirrorLook,
    combatGadgetFeel,
    // Update 38.3
    combatLookLock,
    combatStrikeSteer,
    combatFarWindows,
    combatGadgetHint,
    // Version 0.0.2 (Update 38.6): AIM STRENGTH and BALL HIT (combat38's, contiguous here)
    combatAimStrength,
    combatBallHit,
    // Version 0.0.3 (Update 38.7): ROPE GRAB, ROPE STRENGTH, WALL STICK, AUTO PERFORMANCE, TARGET FPS,
    // QUALITY FLOOR, SHOW STATUS, MIRROR VIEW TEST (combat38's, contiguous here)
    combatRopeGrab,
    combatRopeStrength,
    combatWallStick,
    combatAutoPerf,
    combatPerfTarget,
    combatPerfFloor,
    combatPerfStatus,
    // Version 0.0.4: IRON MAN ABILITIES, MIRROR VIEW (combat38's, contiguous here)
    combatShooterType,
    combatMirrorView,
    // Version 0.0.5: HERO HEIGHT, DAMAGE TEST (combat38's, contiguous here)
    combatHeroHeight,
    combatHeroDamage,
    // Version 0.0.5 (11:24): PUNCH SENSITIVITY, SFX VOLUME, SUPERSONIC, SUPERSONIC SPEED
    combatPunchSense,
    combatSfxVolume,
    combatSupersonic,
    combatSupersonicSpeed,
    // Version 0.0.6: DAREDEVIL ABILITIES, HULK ABILITIES, DAREDEVIL PUNCH
    combatDaredevil,
    combatHulk,
    combatDaredevilPunch,
    // Version 0.0.62: HIT INDICATOR, HAND FLIPS, LENS FLARES (RESTART)
    combatHitIndicator,
    combatHandFlips,
    combatLensFlares,
    // Version 0.0.62 (the SpidyVR comparison): PUNCH HITS, ENEMY TRACKING
    combatPunchHits,
    combatFoeTrack,
    // Version 0.0.62: BEACONS IN VISION (DAREDEVIL)
    combatBeaconsVision,
    // Version 0.0.7 HERO PROFILES (combat38's order): BATMAN, WOLVERINE, SUPERMAN, PUNISHER, BATARANG
    // CHAIN, EASY RELOAD, HOLSTER HANDS, BATON HOLSTERS, BATON LOOK
    combatBatman,
    combatWolverine,
    combatSuperman,
    combatPunisher,
    combatBatarangChain,
    combatEasyReload,
    combatHolsterHands,
    combatBatonHolsters,
    combatBatonLook,
    combatBatarangStyle, // Version 0.0.7 BATARANG STYLE
    combatGunModels,     // Version 0.0.7 GUN MODELS
    combatCaptainAmerica, // Version 0.0.7 CAPTAIN AMERICA: the hero toggle and his shield's five
    combatShieldArm,
    combatShieldDamage,
    combatShieldChain,
    combatShieldBlock,
    combatShieldLook,
    // Version 0.0.72 (TEST): THOR, GREEN LANTERN and the 0.0.72 list (combat38's order).
    combatThor,
    combatThorLightning,
    combatThorStorm,
    combatThorChain,
    combatGreenLantern,
    combatGlFlight,
    combatGlFist,
    combatGlBeam,
    combatGlBubble,
    combatGlChain,
    combatGlCharge,
    combatSpiderClimbOnly,
    combatUppercut,
    combatSmokeBlind,
    combatDdGrapple,
    combatDdParry,
    combatIronHud,
    combatPunisherRage,
    combatReputation,
    combatSurvival,
    combatPunisherSuit,
    combatNaturalInvincible,
    combatHulkRage,
    combatRageBlood,
    combatRageTint,
    combatFireDouse,
    combatShieldSlam,
    combatPunisherHook,
    combatClawClimb,
    combatNpcShove,
    combatFaction,
    combatHeroSuit,
    combatTestUnlocks,
    combatUltimates,
    combatPerkBranch,
    combatOwnSuit,
    combatPerchTakedown, // Version 0.0.72: BATMAN's PERCH TAKEDOWN
    combatTakedownLight,
    combatPunisherVan, // Version 0.0.72 EXPERIMENTAL
    combatThorHammer,  // Version 0.0.72 THOR HAMMER (which Mjolnir)
    combatGameBody,    // Version 0.0.72 GAME BODY (EXPERIMENTAL)
    combatThorHammerModel, // Version 0.0.72 build 6: THOR HAMMER MODEL, THOR FLIGHT (crash bisect keys)
    combatThorFlight,
    combatSuitBody, // Build 9 SUITS: SUIT BODY
    combatCapSprint, // Version 0.0.73: CAP SPRINT
    combatCullMargin, // 0.0.73 test 2 (fixes 9, 13): CULL MARGIN, the game view's degrees past the head's lens, 10 by default
    combatShadowMatch, // 0.0.73 test 2 (fix 21): SHADOW MATCH (CANDIDATE), both eyes' cascades fitted to one lens, ON by default since test 3
    combatCalibLook, // Version 0.0.73 test 2 HOLO CALIBRATION (fix 25): HOLO checklist with belt, back and hip touches, or CLASSIC
    combatFlyPose, // Version 0.0.73 test 2 FLIGHT POSE (fix 27): LEAN by speed (fly_lean.hpp) or UPRIGHT (0.0.72)
    combatWeaponCollision, // 0.0.73 test 3 WEAPON COLLISION: thrown and held weapons meet walls, cars and the ground, ON
    combatLightHold, // 0.0.73 test 3 LIGHT HOLD: steadier street lights (EYE AA step + bright history hold), ON, STRONG, OFF
    combatVanScreen, // Version 0.0.73 test 3 VAN SCREEN: the Punisher van's dash screen ON or OFF
    combatPalmBlast, // 0.0.73 test 3 PALM BLAST: IRON MAN fires out of the open palm (PALM, on) or along the hand (AIM)
    combatHeroMenu, // 0.0.73 test 3 HOLO ROSTER (fix 28): the wrist ROSTER row opens the hero tiles on the hologram (HOLO TILES, on) or the big popup (CLASSIC)
    combatRepLog, // 0.0.73 test 3 REP PAGE: each rep change logged for the hologram's REP tab (ON) or not (OFF)
    combatVanPedals, // 0.0.73 test 3 VAN PEDALS: WHEEL HAND (on) or CLASSIC (the 0.0.72 pedals)
    combatImage,     // 0.0.73 IMAGE: SPIDYVR (sharp, the default) or HERO VR CLASSIC (test 3)
    combatImageStyle, // 0.0.73 test 4 IMAGE STYLE, shown as GAME TAA: SPIDYVR (the game's TAA per eye, default) or OFF (IMAGE's look)
    combatDdBatonTip, // 0.0.73 DD BATON: the baton seated in the fist and its tip flown on a rope (ON) or test 3's (OFF)
    combatStreamerMode, combatStreamerSmooth, combatStreamerEye, // 0.0.73 STREAMER MODE (streamer_view.hpp)
    combatIronHands,      // 0.0.73 HERO HANDS: IRON MAN's own hands (ON) or test 4's shared ones (OFF)
    combatDdGrappleAim,   // 0.0.73 GRAPPLE AIM: DAREDEVIL's baton tip turned to the aim while grappling (ON) or not
    combatWolverineClaws, // 0.0.73 CLAW LOOK: WOLVERINE's steel claws between the knuckles (ON) or test 4's (OFF)
    // In combat38::Option order (combatOption is the offset from combatCrawlCap).
    combatLightChecks, // 0.0.73 LIGHT CHECKS (eye_queries.hpp): the eyes' light-glow query room, ON or OFF (next VR start)
    combatGlowCheck,   // 0.0.73 GLOW CHECK (eye_queries.hpp glow_check): GAME, ALWAYS or HIDDEN light glows, live (debug)
    combatSlWatch,     // 0.0.73 SL WATCH (sl_watch.hpp): the game's Streamline constants logged for the export, live
    combatMpSoloTest, // 0.0.73 MP SOLO TEST: the loopback ghost / MP Test Buddy as PLAYER 2 (ON) or OFF
    combatSharpenMatch, // 0.0.73 AA SHARPEN MATCH (eye_sharpen.hpp matchedPercent): CAS by the history kept (ON) or as set, live
    combatClawLevel,    // 0.0.73 test 6 CLAW LEVEL (suit_wear.hpp clawLevelDeg): WOLVERINE's claws level with the hand (ON) or tilted, live
    combatWindowPop,    // 0.0.73 test 6 WINDOW POP FIX (window_pop.hpp): the game view's flat-play detail distance (ON) or not, live
    combatMpPlayer2Body, // 0.0.73 MPSUIT PLAYER 2 BODY (mp_body.hpp): PLAYER 2 with no mod suit model as a hero body (SUIT LOOK) or the stick figure, live
    combatComingSoon,  // 0.0.73 COMING SOON (holo_roster.hpp): the hero tiles' SOON tab shown (ON) or hidden (OFF), live
    combatHideouts,     // 0.0.74 test 1 HIDEOUTS (hideout074.hpp): the van dash's GO TO SAFEHOUSE / BATCAVE (ON) or not; ON needs a restart
    combatCarMusic,     // 0.0.74 test 1 CAR MUSIC (vehicle074.hpp): IN VEHICLES, ALWAYS or OFF, live
    combatSeatedDriving, // 0.0.74 test 1 SEATED DRIVING (vehicle074.hpp): SIT BUTTON, AUTO SIT or OFF, live
    // Update 38.5: page 7 COLOR, 90s ANIMATED's own (eye_color.hpp toon): CARTOON STRENGTH (a live
    // slider), OUTLINES (a press, ON/OFF) and BANDS (a press, 3 -> 4 -> 5). Greyed (N/A) unless
    // FILTER is 90s ANIMATED. Result::colorSlider / colorOutlines / colorBands.
    colorCartoon,
    colorOutlines,
    colorBands,
    // Update 38.5: BLOOD SPLATTER EXTREME's VISOR DROPS (a press, ON/OFF, default OFF; N/A below
    // EXTREME). Result::bloodVisor.
    bloodVisor,
    // Update 38.5: page 8 EXTRAS (extras385.hpp), each OFF by default and live: INVINCIBLE (a press,
    // ON/OFF; N/A when the hero's health record is not the confirmed one), FLY MODE (OFF -> SLOW ->
    // NORMAL -> FAST), FLY VIGNETTE (ON/OFF), SYMBIOTE LOOK (ON/OFF), PAD MODE (the pad layer on or
    // off by hand), PROMPT HANDLING (MANUAL / AUTO; N/A while the prompts are not hooked) and PLAY
    // ON MONITOR (a press to arm, a second to go). Result::invincible ... playOnMonitor.
    invincible,
    flyMode,
    flyVignette,
    // Update 38.5 (owner 4:52): FLY ROLL (OFF / COMFORT / FULL), ROLL SPEED (a slider, degrees a
    // second), AUTO LEVEL (ON/OFF); greyed while FLY MODE (or FLY ROLL) is off.
    flyRoll,
    flyRollSpeed,
    flyAutoLevel,
    symbiote,
    padMode,
    promptHandling,
    playOnMonitor,
    // Update 38.5: GESTURE MOVES (gestures385.hpp): OFF -> TRAINING -> ON, live, default OFF.
    gestureMoves,
    // Update 38.5 GESTURE REMAP (gesture_session.hpp): presses, never kept in live.txt.
    gestureRemapMove,
    gestureRecord,
    gestureKeep,
    gestureAgain,
    gestureCancel,
    gestureResetMove,
    gestureResetAll,
    gestureProfile,
    // Version 0.0.2 TRAIN MOVE (training_guide.hpp): the move TRAINING draws a path for; a press
    // steps to the next one. Never kept in live.txt.
    trainMove,
    // Update 39.0.1 (owner: "give all the hero profiles their own page in holo"): the hero picker,
    // one tab a hero page (SPIDER-MAN, IRON MAN, DAREDEVIL, HULK), and PLAY AS SPIDER-MAN on
    // SPIDER-MAN's page. Navigation and a press: never kept in live.txt, past the mask.
    heroTabSpider,
    heroTabIron,
    heroTabDaredevil,
    heroTabHulk,
    heroPlaySpider,
    // Update 39.0.2 (owner: "the holo menus need to be cleaner and better organized"): a page's
    // section headers, each a press opening its group of settings (and closing the page's other
    // one). Navigation: never kept in live.txt, past the mask. The hero tabs above stay unused so
    // every index holds.
    section0,
    section1,
    section2,
    section3,
    section4,
    section5,
    section6, // Version 0.0.7: POWERS has 9 sections (FLYING and eight heroes); CAPTAIN AMERICA: 10
    section7,
    section8,
    section9,
    section10, // Version 0.0.72: THOR
    section11, // Version 0.0.72: GREEN LANTERN
    heroRoster, // Build 8: ROSTER: HEROES AND SUITS (opens the roster page, roster_page.hpp)
};
constexpr int controls = 256; // 0.0.74 test 1: + HIDEOUTS, CAR MUSIC, SEATED DRIVING (the uint8_t Control is full); 0.0.73: + COMING SOON; // 0.0.73 MPSUIT: + PLAYER 2 BODY; // 0.0.73 test 6: + CLAW LEVEL, WINDOW POP FIX; // 0.0.73 AA: + SHARPEN MATCH; // 0.0.73 test 6: + MP SOLO TEST (mp-solo-test merged); // 0.0.73 test 5: + HERO HANDS (IRON MAN), GRAPPLE AIM (DAREDEVIL), CLAW LOOK, LIGHT CHECKS, GLOW CHECK, SL WATCH (WOLVERINE); // 0.0.73 test 4: + IMAGE STYLE (GAME TAA), BATON TIP (DAREDEVIL), STREAMER MODE, STREAMER SMOOTH, STREAMER EYE; // 0.0.73: + IMAGE; // 0.0.73 test 3: + VAN PEDALS; // 0.0.73 test 3: + REP LOG; // 0.0.73 test 3: + HERO MENU; // 0.0.73 test 3: + PALM BLAST; // 0.0.73 test 3: + VAN SCREEN; // 0.0.73 test 3: + LIGHT HOLD; // 0.0.73 test 3: + WEAPON COLLISION; // 0.0.73 test 2: + FLIGHT POSE; // 0.0.73 test 2: + CALIBRATION LOOK; // 0.0.73 test 2: + SHADOW MATCH; // 0.0.73 test 2: + CULL MARGIN; // 0.0.73: + CAP SPRINT; // build 9: + SUIT BODY; build 8: + ROSTER
// Version 0.0.72: + 40 combat rows, + 2 sections; build 6: + THOR HAMMER MODEL, THOR FLIGHT
constexpr int combatControls = 162; // 0.0.74 test 3: + WHEEL FIRST; 0.0.74 test 3: + JITTER GUARD; 0.0.74 test 3: + HEAP GUARD; 0.0.74 test 2b: + TAA DIAGNOSTICS, CRIME GPS, TAA HOLD; 0.0.74 test 2: + SPIDYVR PICTURE, FLARE BLOCK, CABIN CLEAR, CAR PAINT, SEATED TOOLS, DASH POINTER; 0.0.74 test 1: + BATMOBILE, BATMOBILE CHOICE; + HIDEOUTS, CAR MUSIC, SEATED DRIVING; 0.0.73: + COMING SOON; // 0.0.73 MPSUIT: + PLAYER 2 BODY; // 0.0.73 test 6: + CLAW LEVEL, WINDOW POP FIX; // 0.0.73 AA: + SHARPEN MATCH; // 0.0.73 test 6: + MP SOLO TEST (mp-solo-test merged); // 0.0.73 test 5: + HERO HANDS, GRAPPLE AIM, CLAW LOOK, LIGHT CHECKS, GLOW CHECK, SL WATCH; // 0.0.73 test 4: + IMAGE STYLE (GAME TAA), BATON TIP (DAREDEVIL), STREAMER MODE, STREAMER SMOOTH, STREAMER EYE; // 0.0.73: + IMAGE; // 0.0.73 test 3: + VAN PEDALS; // 0.0.73 test 3: + REP LOG; // 0.0.73 test 3: + HERO MENU; // 0.0.73 test 3: + PALM BLAST; // 0.0.73 test 3: + VAN SCREEN; // 0.0.73 test 3: + LIGHT HOLD; // 0.0.73 test 3: + WEAPON COLLISION; // 0.0.73 test 2: + FLIGHT POSE; // 0.0.73 test 2: + CALIBRATION LOOK; // 0.0.73 test 2: + SHADOW MATCH; // 0.0.73 test 2: + CULL MARGIN; // 0.0.73: + CAP SPRINT; // combat38::options (checked in wrist_hud.cpp); build 9: + SUIT BODY
// 0.0.74 test 1: the options with a wrist row (the uint8_t Control is full; BATMOBILE and
// BATMOBILE CHOICE live in the launcher's settings and the Batmobile's dash only).
constexpr int combatRows = 148;
static_assert(combatRows <= combatControls);
// Update 38: a COMBAT row's combat38::Option (-1: not one).
inline int combatOption(Control c) {
    const int i = static_cast<int>(c) - static_cast<int>(Control::combatCrawlCap);
    return i >= 0 && i < combatRows ? i : -1;
}
// The controls that can have a bit (1 << Control) in a 32-bit mask (changed,
// disabled, due): every setting is below it.
constexpr int maskedControls = 32;
static_assert(static_cast<int>(Control::eyeTest) < maskedControls, "a setting's bit must fit a 32-bit mask");
static_assert(static_cast<int>(Control::fpsReset) >= maskedControls, "only presses past the mask");
static_assert(static_cast<int>(Control::addressFinder) >= maskedControls, "only presses past the mask");
static_assert(static_cast<int>(Control::eyeLod) >= maskedControls, "only presses past the mask");
static_assert(static_cast<int>(Control::shadowCamera) >= maskedControls, "only presses past the mask");
static_assert(static_cast<int>(Control::shadowDistance) >= maskedControls, "a live slider past the mask");
static_assert(static_cast<int>(Control::eyeAa) >= maskedControls, "only presses past the mask");
static_assert(static_cast<int>(Control::engineTaa) >= maskedControls, "only presses past the mask");
static_assert(static_cast<int>(Control::eyeTrail) >= maskedControls, "a live slider past the mask");
static_assert(static_cast<int>(Control::taaJitter) >= maskedControls, "only presses past the mask");
static_assert(static_cast<int>(Control::farShadows) >= maskedControls, "only presses past the mask");
static_assert(static_cast<int>(Control::eyeBloom) >= maskedControls, "only presses past the mask");
static_assert(static_cast<int>(Control::colorStrength) >= maskedControls, "a live slider past the mask");
static_assert(static_cast<int>(Control::foveaInner) >= maskedControls, "a live slider past the mask");
static_assert(static_cast<int>(Control::heroTabSpider) >= maskedControls, "only presses past the mask");
static_assert(static_cast<int>(Control::section0) >= maskedControls, "only presses past the mask");
static_assert(static_cast<int>(Control::heroRoster) == controls - 1);
// Each the value of the live.txt key it is named for, as the mod runs now.
struct Settings {
    float widthCm = 1.3f; // web_width
    float speed = 180;    // web_speed, m/s
    float aa = 0;         // aa: 0 off, 1 FXAA
    float history = 0;    // history, percent
    float reach = 150;    // reach, percent
    float monitor = 0;    // monitor, refreshes a second (0 every frame)
    float supersample = 100; // the next launch's eye supersample (next-launch.txt), percent
    float sharpen = 0;    // sharpen, percent (0 off: no pass)
    float reticles = 1;   // reticles: 1 the aim reticles shown, 0 hidden (aiming is the same)
    // Update 17: the hologram itself.
    float hudHeight = 9;     // hud_height: cm from the light up to the panel's lower edge
    float hudScale = 1;      // hud_scale: the whole hologram's size
    float hudText = 100;     // hud_text: the pages' text size, percent
    float hudTheme = 0;      // hud_theme: a colour theme (themeName, 0-5)
    float hudTextColor = 0;  // hud_text_color: 0 the theme's own text colours (textColorName)
    // Update 18: the webs' look.
    float webStyle = 1;    // web_style: 0 90s, 1 MESSY (Update 20: the default), 2 MODERN, 3 SYMBIOTE, 4 ENERGY
    // web_color_90s, web_color_messy, ...: each style's shade, remembered
    // per style; Control::webColor is the one of the style chosen now.
    static constexpr int webStyles = 5;
    float webColors[webStyles] = {};
    // Update 19: the music's.
    float musicVolume = music::defaultVolume; // music_volume, percent
    float musicVisualizer = 0;                // music_visualizer: 0-4 (music::visualizerName)
    float eyeTest = 0; // Update 24: eye_debug, 0 off (normal 3D), 1 both eyes the left eye's picture
    float shadowDistance = 0; // Update 33: car lengths, 0 the game's own (never kept; not in live.txt)
    float eyeTrail = 0;       // Update 34.2: EYE AA TRAIL, percent (live.txt trail); Update 35: 0 (was 10)
    // Update 37.2: the COLOR page's sliders (eye_color::Config's, exposure in EV) and INNER SIZE.
    float colorStrength = 100, exposure = 0, contrast = 100, saturation = 100, vibrance = 0, warmth = 0, tint = 0,
          vignette = 0;
    float colorCartoon = 60; // Update 38.5: 90s ANIMATED's CARTOON STRENGTH (eye_color::Config::cartoon)
    float flyRollSpeed = 90; // Update 38.5: FLY's ROLL SPEED, degrees a second
    float foveaInner = 100;
    float& operator[](Control);
    float operator[](Control c) const {
        return const_cast<Settings&>(*this)[c];
    }
};
// A slider's ends and step. web_speed takes 20-2000 in live.txt; its slider
// offers 60-400 (a value outside shows the knob at that end).
struct Range {
    float low, high, step;
};
constexpr float minWidthCm = .4f, maxWidthCm = 4, widthStep = .1f;
constexpr float minSpeed = 60, maxSpeed = 400, speedStep = 10;
Range range(Control);
// Update 39.0.2 (owner: "Not every hero needs its own page ... one 'powers' page that has flight,
// invincibility and hero profiles with sub menus"): eight pages, the settings in sections.
constexpr int pages = 8;
constexpr int statusPage = 0;
constexpr int powersPage = 1;   // INVINCIBLE, FLYING and the HERO PROFILES (one section a hero)
constexpr int vrPage = 2;       // VR SETTINGS: RECALIBRATE, body, webs, hologram, mirror, sound, tests
constexpr int combatPage = 3;   // COMBAT (combat38.hpp's rows, grouped)
constexpr int graphicsPage = 4; // GRAPHICS: the FPS rows, quality, performance, display, tests, NEXT LAUNCH
constexpr int colorPage = 5;    // COLOR / ATMOSPHERE (eye_color.hpp)
constexpr int controlsPage = 6; // CONTROLS: how to play, gestures, remap, gamepad
constexpr int musicPage = 7;
// Update 39.0.2: a page's sections, one open at a time (an accordion; a closed section's rows are
// not laid out, so they are neither drawn nor pressed). What is open is kept for the session (not
// in live.txt), shared by the input and the drawing side; until a section is pressed each page
// opens its first (COLOR none; POWERS the hero being played).
constexpr int sectionsMost = 12; // Version 0.0.72: THOR, GREEN LANTERN (0xE/0xF still free) // Version 0.0.7 (was 6; CAPTAIN AMERICA 10); a nibble a page still holds it
int sectionCount(int page);
const char* sectionName(int page, int section); // "" past the page's sections
int openSection(int page);                      // -1: none open
void setOpenSection(int page, int section);     // -1 (or not one of the page's): close them all
void resetSections();                           // every page back to its default
void setActiveHero(int hero);                   // hero_modes::Hero: POWERS' default section
int activeHero();
const char* activeHeroName(int hero); // Version 0.0.72 build 6: hero_modes::heroNames, bounds-checked (SPIDER-MAN otherwise)
inline Control sectionControl(int section) {
    return static_cast<Control>(static_cast<int>(Control::section0) + section);
}
inline int sectionIndex(Control c) {
    const int i = static_cast<int>(c) - static_cast<int>(Control::section0);
    return i >= 0 && i < sectionsMost ? i : -1;
}
// The section a control is in on its page (-1: always shown, or not on any page).
struct Place {
    int page = -1, section = -1;
};
Place placeOf(Control c);
// Update 39.0.2: a gold RESTART tag beside the name (EYE SUPERSAMPLE, MIRROR VIEW).
bool restartTag(Control c);
const char* pageName(int page);
// The player mirror (Update 37: built, vr_mirror.hpp). The greyed row stays only for
// the game-music mute on the MUSIC page, which reuses Kind::mirror.
constexpr const char* mirrorText = "MIRROR";
// Update 17: the text size as a factor (hud_text / 100, held to .75-1.75;
// not finite: 1). The settings pages are laid out for it: bigger text wraps
// onto more lines and the page scrolls.
float textFactor(float textScale);
// Where a control is on the panel (u right, v up from its middle, metres at
// scale 1) on `page` (0-3), its content scrolled up by `scroll` (Update 17)
// and laid out for `text` (a textFactor); none off them all, and none for a
// control scrolled out of the inner area (viewBottom-viewTop).
Control controlAt(int page, float u, float v, float scroll = 0, float text = 1);
// Whether (u, v) is on the panel or its arrow strip under it.
bool onPanel(int page, float u, float v);
// The panel's top edge (v, scale 1). Update 17: panelHeight / 2 on every
// page (Update 16's pages grew upward; now they scroll instead).
float panelTop(int page);
// Update 17: the inner area a page's content is clipped to, the strip and
// the frame not; and the scrollbar's grab zone across (it is drawn at its
// middle).
constexpr float viewTop = .22f / 2 - .015f, viewBottom = -.22f / 2 + .005f, scrollbarU0 = .0765f,
                scrollbarU1 = .0835f;
// How far a page's content runs below the inner area's top at `text`, and
// how far it can scroll (0 when it fits).
float contentHeight(int page, float text = 1);
float maxScroll(int page, float text = 1);
float clampScroll(int page, float scroll, float text = 1);
// The scrollbar's thumb (shown only when the page scrolls), v at scale 1.
struct Thumb {
    bool shown;
    float v0, v1;
};
Thumb scrollThumb(int page, float scroll, float text = 1);
bool onScrollbar(int page, float u, float v, float text = 1);
// Clips the triangles of out[from, end) to the panel's band low <= v <=
// high (Frame f, scale): parts outside are cut away, nothing past it drawn.
struct Frame;
void clipBand(std::vector<Vertex>& out, size_t from, const Frame& f, float scale, float low, float high);
// Update 16: a control's plain-English explanation, drawn under it in
// smaller, dimmer text wrapped to helpWidth (mirrorHelp: the greyed mirror's).
const char* helpText(Control);
constexpr const char* mirrorHelp = "Not built yet: would put a mirror in front of you so you can see yourself.";
// Update 19: muting the game's own music while yours plays is not built
// (SM2TODO: no music volume of the game's is confirmed that can be changed
// while it runs - BUILD-SM2.md section 28). Greyed on the MUSIC page like the
// mirror, not a control.
constexpr const char* gameMusicText = "MUTE GAME MUSIC: COMING SOON";
constexpr const char* gameMusicHelp = "Not built yet: for now set Music Volume to 0 in the game's Settings, Audio.";
// Update 16: the FPS monitor on page 2 (not a control: nothing to press).
// Update 24: the session HIGH / LOW / AVG boxes and RESET under the FPS monitor.
constexpr const char* fpsStatsHelp = "Per-second rates from 10 s after load; loads and pauses skipped; RESET restarts.";
// Update 27: one sentence, <= 80 chars (standing subtext rule).
constexpr const char* addressFinderHelp =
    "Scans for missing shadow and anti-aliasing data; play normally for a minute.";
static_assert(sizeof("Scans for missing shadow and anti-aliasing data; play normally for a minute.") - 1 <= 80);
static_assert(sizeof("Scans for missing shadow and anti-aliasing data; play normally for a minute.") - 1 == 76);
// Update 28: EYE LOD, one sentence, <= 80 chars (standing subtext rule).
constexpr const char* eyeLodHelp = "Draws model detail and textures as far out as the flat game does (test).";
// Update 28.1: EYE LOD's subtext once it failed: "Failed: <reason>." (one
// sentence, at most 80 characters, no more lines than eyeLodHelp).
const char* eyeLodFailedHelp(uint32_t reason);
// Update 33: EYE AA failed: why (eye_taa::reasonText), one sentence, <= 80.
const char* eyeAaFailedHelp(uint32_t reason);
// Update 34: ENGINE TAA off with a reason: "Off: <reason>." (engine_taa::reasonText), <= 80.
const char* engineTaaOffHelp(uint32_t reason);
static_assert(sizeof("Draws model detail and textures as far out as the flat game does (test).") - 1 <= 80);
// Update 31: SHADOW CAMERA's subtext is shadow_camera::help (<= 80 chars);
// once it went back to OFF on its own: "Back to OFF: <reason>."
const char* shadowCameraRevertedHelp(uint32_t reason);
const char* farShadowsHelp(uint32_t packed); // Update 35: FAR SHADOWS' subtext for its state
const char* eyeBloomHelp(uint32_t packed);   // Update 36: EYE BLOOM's subtext for its state
const char* sharedLodHelp(uint32_t packed);  // Update 36: SHARED LOD's subtext for its state
constexpr const char* fpsHelp = "New frames per second you're actually getting, versus your headset's refresh rate.";
// Update 17: the FPS band (fpsBand: 2 good, 1 low, 0 poor, -1 unknown) as
// a word beside the number, so no theme leans on colour alone.
const char* fpsWord(int band);
// Update 17: the colour themes (hud_theme 0-4) and text colours (0-3).
// Update 36: and 5, "2000s Raimi" (the 2004 Spider-Man 2 handheld pause
// menu's colours, a spider watermark and the pixel font, pixel_font.hpp).
constexpr int themes = 6, textColors = 4, raimiTheme = 5;
const char* themeName(int theme);     // "SPIDER-MAN RED/BLUE", ..., "2000s Raimi"
const char* themeKey(int theme);      // live.txt's: "spiderman", "contrast", "deutan", "tritan", "mono", "raimi"
// COLOR THEME's subtext while 2000s Raimi is on.
constexpr const char* raimiHelp = "The 2004 handheld game's look: orange and yellow pixel text on navy.";
static_assert(sizeof("The 2004 handheld game's look: orange and yellow pixel text on navy.") - 1 <= 80);
const char* textColorName(int color); // "THEME", "WHITE", "YELLOW", "CYAN"
const char* textColorKey(int color);  // "theme", "white", "yellow", "cyan"
// Linear RGB. frame: the outline and spider; accent: lines, tracks, the
// figure; line: bright strokes; text: titles and button labels; label:
// control names; value: numbers; help: the explanations; backing: the glass
// (backingAlpha); good, warn, bad: the FPS bands (and warnings, notes).
struct Palette {
    Vec3 frame, accent, line, text, label, value, help, backing, good, warn, bad;
    float backingAlpha;
};
Palette palette(int theme, int textColor);
constexpr float helpHeight = .0036f, helpPitch = .0054f, helpWidth = .132f;
constexpr int helpLinesMost = 2;
// Update 38.5: a control's subtext shows at most helpRowsShown lines; a longer
// one (a big TEXT SIZE, a state's longer text) scrolls inside them - the
// thumbstick while the control is pointed at, a finger dragged on the text, or
// on its own after helpAutoDelay seconds of pointing - with a fade and a small
// arrow and bar in a gutter (helpGutter) at its right. Nothing is cut off.
constexpr int helpRowsShown = 3;
constexpr float helpGutter = .005f, helpAutoDelay = 1.5f;
// The width a line of subtext takes in either font (the strokes, or the Raimi
// theme's pixel font, which is never squeezed below 0.7 of its natural width).
float helpLineWidth(const std::string& line, float height);
// `text` broken at spaces into lines no wider than `width` at `height` (in
// either font; Update 38.5: a word longer than the width is split, never
// left to run off the panel).
std::vector<std::string> wrapText(const std::string& text, float height, float width);
// Where a control's middle is drawn on `page` (for hit tests, previews and
// tests); found false if the page has no such control.
// Update 17: at `scroll` and `text`; visible false when scrolled out of
// the inner area (it can then be neither seen nor pressed).
struct Spot {
    bool found;
    float u, v;
    bool visible = true;
};
Spot controlSpot(int page, Control, float scroll = 0, float text = 1);
// 0.0.73 test 4 HERO SETTINGS: with the holo tiles the old POWERS list (a dropdown per hero) is gone and
// its rows are on each hero's tile page, SETTINGS sub-tab: the hero's own rows (his old profile's but
// its ON switch - PLAY AS is that), then EVERY HERO's (INVINCIBLE and FLYING), a short page at a time.
uint32_t heroSettingsPages(uint32_t hero);
std::vector<Control> heroSettingsRows(uint32_t hero, uint32_t page);
bool heroSettingsEveryHero(uint32_t hero, uint32_t page); // the page is EVERY HERO's
Control heroSettingAt(uint32_t hero, uint32_t page, float u, float v);
Spot heroSettingSpot(uint32_t hero, uint32_t page, Control); // where to point to press it
// Every piece drawn on a page's panel (titles, labels, tracks, buttons,
// explanation lines, headings), as rectangles: none may overlap another.
struct Area {
    float u0, v0, u1, v1;
    const char* what;
};
// Update 17: at `text`, unscrolled (content below viewBottom included).
std::vector<Area> pageAreas(int page, float text = 1);
// A slider's value where the pointer is along its track, stepped.
float sliderValue(Control, float u);
// A control's value as the panel prints it ("1.3 CM", "180 M/S", "FXAA").
std::string valueText(Control, float value);
// Its short name for notes ("AA", "REACH").
const char* shortName(Control);
// Update 15 (hard rules): a setting that only takes effect when the game is
// started again. The panel labels it RESTART REQUIRED, shows "RESTART THE
// GAME TO APPLY" once it differs from the running value, and never applies
// it live: it is only stored for the launcher (tools/run_game_vr.py,
// next-launch.txt). Only supersample is.
bool restartRequired(Control);
// Update 17: settings that move or resize the panel (hud_height, hud_scale,
// hud_text): applied only when their slider is let go, so the panel never
// shifts under the pointer that drags it.
bool onRelease(Control);
// Update 17: the buttons a press steps through their values (COLOR THEME,
// TEXT COLOR), wrapping round. Update 18: WEB STYLE and ENERGY COLOR too.
bool cycles(Control);
// Update 18: the cycles drawn as < NAME > (WEB STYLE, WEB COLOR): a
// press on the button's left half steps back, on its right half forward.
bool arrows(Control);
// Update 18: WEB COLOR's list is the style's own (web_style::shades): how
// many there are and which is picked for the style chosen now. The row is
// always there, one line, so nothing on the page moves when the list swaps.
int webStyleOf(const Settings&);
int shadeCount(const Settings&);
// A control's value as the panel prints it, for controls whose words depend
// on another setting (WEB COLOR names its style's shade); else valueText.
std::string valueText(Control, const Settings&);
// Update 19: the MUSIC page's buttons that do something to the player
// (none of them a setting: they are never stored).
bool musicButton(Control);
// The song list as it opens under the dropdown: listRows rows at most, each
// listRowHeight (times the text factor) tall. The row (counting from the
// list's first, `first`) the pointer is on at (u, v) with the page scrolled
// by `scroll`: -1 off the list. `tracks` the songs there are.
constexpr int listRows = 6;
float listRowHeight(float text = 1);
int listRowAt(float u, float v, float scroll, float text, int first, int tracks);
// Where on the progress bar (0..1) u is.
float seekFraction(float u);
// Settings the eye pass reads on the render thread (aa, history, reach,
// monitor, sharpen): applied when the slider is let go, or at most every
// Applier::gpuInterval while it is dragged.
bool gpuSide(Control);
// Clamps to the control's range; a value not finite is its range's low end.
float clampTo(Control, float value);

// Update 15 (hard rules): between the panel and the mod. A value the panel
// sets waits here (clamped) until it is due; the game then applies it at the
// start of its frame (game_xr.cpp, the input stage: never mid-submit) and
// reports whether it took and the value the mod now has. A refused value is
// reverted to the last one that took (revert()), with a short red note on the
// panel; a control refused failuresToDisable times in a row is disabled for
// the session (its value stays the last good one). Nothing here does I/O.
class Applier {
  public:
    static constexpr double gpuInterval = .25, noteSeconds = 3;
    static constexpr int failuresToDisable = 2;
    // The values the mod has at the start; they count as good.
    void start(const Settings& live);
    void want(Control, float value);
    // Bits (1 << Control) of the values to apply now: a CPU-side or
    // restart-required one at once, a GPU-side one once `held` is not it or
    // gpuInterval after the last, an onRelease one once `held` is not it.
    uint32_t due(double now, Control held) const;
    // After applying: whether the mod took it, and the value it has now. False
    // when it did not take: then apply revert(c) (the last good value).
    bool result(Control, bool ok, float actual, double now);
    float revert(Control c) const {
        return good_[static_cast<unsigned>(c)];
    }
    // What the panel shows: a waiting value, else the mod's.
    Settings shown(const Settings& live) const;
    bool waiting(Control c) const {
        return dirty_ >> static_cast<unsigned>(c) & 1;
    }
    uint32_t disabled() const {
        return disabled_;
    }
    // The red note, empty after noteSeconds.
    std::string note(double now) const;
    void fault(const std::string& note, double now) {
        note_ = note;
        noteAt_ = now;
    }

  private:
    std::array<float, controls> wanted_{}, good_{};
    std::array<double, controls> last_ = [] {
        std::array<double, controls> a{};
        a.fill(-1e9);
        return a;
    }();
    std::array<uint8_t, controls> failures_{};
    uint32_t dirty_{}, disabled_{};
    std::string note_;
    double noteAt_ = -1e9;
};

// Update 19: what the MUSIC page shows of the player (music_audio.hpp's
// status and the newest Spectrum), and the song list's state (Menu).
struct MusicView {
    bool available = true; // false: the player is off this session (its note says why)
    int tracks{}, track = -1; // track from 0; -1 none picked
    bool playing{}, paused{};
    float elapsed{}, total{}; // seconds; total 0: not known
    std::string song;   // the song's shown name
    std::string note;   // "SKIPPED: ...", "NO AUDIO DEVICE": the status line
    std::string folder; // the Music folder, shown when there are no songs
    // The open list's rows from listFirst (only those shown are needed).
    bool listOpen{};
    int listFirst{}, listHover = -1;
    std::vector<std::string> rows;
    music::Spectrum spectrum;
};
struct View {
    WristHudData data;
    Vec3 emitter{};          // the light on the wrist, in the world
    Vec3 headUp{0, 1, 0};    // the headset's up: the panel stands along it
    float heading{};         // the headset's yaw, radians clockwise from the world's -z
    float opacity = 1;       // Toggle::opacity
    float scale = 1;         // live.txt hud_scale
    float seconds{};         // a clock, for the light's slow pulse
    // Update 15: the page shown, its settings, the control pointed at and
    // the one held down (Menu), and the pointer: from the controller's tip
    // to where it meets the panel.
    int page{};
    Settings settings;
    Control hover{}, held{};
    bool pointing{};
    Vec3 tip{};
    float pointU{}, pointV{};
    uint32_t disabled{};          // Applier::disabled: shown UNAVAILABLE
    std::string note;             // Applier::note: red, in the arrow strip
    float supersampleNow = 100;   // the running eye supersample
    // Update 16: the FPS monitor (FpsMeter, fed by game_xr from real
    // telemetry); NaN shows "--". eyeGpuMs: the eye passes' GPU time a pair
    // (AA or downsample, history, sharpen; native_eye_gpu timestamps).
    float fps = std::numeric_limits<float>::quiet_NaN(), refreshHz = std::numeric_limits<float>::quiet_NaN(),
          eyeGpuMs = std::numeric_limits<float>::quiet_NaN();
    // Update 24: the session's (FpsSession); NaN shows "--".
    float fpsHigh = std::numeric_limits<float>::quiet_NaN(), fpsLow = std::numeric_limits<float>::quiet_NaN(),
          fpsAvg = std::numeric_limits<float>::quiet_NaN();
    // Update 27: ADDRESS FINDER (native_address_finder): 0 idle, 1 running, 2 done, 3 failed.
    uint32_t addressFinderStatus{};
    uint32_t addressFinderSeconds{};     // seconds left while running
    uint32_t addressFinderCandidates{};  // count when done
    uint32_t addressFinderAlmost{};      // Update 28: "almost" rows when done
    uint32_t addressFinderShadow{};      // Update 30: shadow pass candidates when done
    // Update 28: EYE LOD (SpidyEyeLodNow): 0 off, 1 on, 2 off (failed this session).
    uint32_t eyeLodStatus{};
    uint32_t eyeLodReason{}; // Update 28.1: native_view::EyeLodReason when failed
    // Update 31: SHADOW CAMERA (SpidyShadowCameraNow): the mode, and in bits
    // 8-15 why it last went back to OFF on its own (shadow_camera::Reason).
    uint32_t shadowCamera{};
    // Update 33: SHADOW DISTANCE (native_shadow_distance::now): shadow_distance::pack(state,
    // reason); the game's own reach and the live one, metres (0 unknown).
    uint32_t shadowDistance{};
    float shadowDistanceGame{}, shadowDistanceLive{};
    // Update 33: EYE AA, eye_taa::pack(state, reason), and its blend's GPU ms (NaN unknown).
    uint32_t eyeAa{};
    float eyeAaMs{};
    // Update 34: ENGINE TAA, engine_taa::pack(state, reason).
    uint32_t engineTaa{};
    uint32_t taaJitter{}; // Update 34.2: engine_taa::JitterMode in force; 0.0.73 TAA SHAKE: packJitter (the scale too)
    uint32_t farShadows{}; // Update 35: far_shadows::pack(mode, why it changed on its own)
    uint32_t eyeBloom{};   // Update 36: eye_bloom::pack(mode, why it changed on its own)
    uint32_t mirror{};     // Update 37: bit 0 on, bits 8-31 the drawing's cost in microseconds
    uint32_t mirrorViewWhy{}; // MIRROR TEST: why MIRROR VIEW turned itself OFF (mirror_view::Why; 0 it did not)
    uint32_t worldLog{};   // Update 37: bit 0 on, bit 1 stopped (a fault), bits 8-31 events logged
    uint32_t blood{};      // Update 37: BLOOD SPLATTER intensity percent (0 OFF)
    bool bloodVisor{};     // Update 38.5: EXTREME's VISOR DROPS on
    // Update 38.5 EXTRAS: each row's state (extras385.hpp).
    bool invincible{}, invincibleNa{}, flyVignette{}, symbiote{}, padMode{}, padByPrompt{}, monitorArmed{};
    uint32_t flyMode{};        // extras385::fly::Speed
    uint32_t flyRoll = 1;      // extras385::fly::RollMode (1 COMFORT, the default)
    bool flyAutoLevel = true;
    uint32_t promptHandling{}; // extras385::prompts::Handling
    uint32_t gestureMoves{};   // gestures385::Mode
    const char* trainMove = "";  // Version 0.0.2 TRAIN MOVE: the picked move's name ("" none)
    bool trainMine{};            // and whether it is the player's own recorded move (MINE)
    const gesture_session::View* remap{}; // Update 38.5 GESTURE REMAP's rows (null: nothing recorded yet)
    uint32_t mirrorAnchor{}; // Update 37.2: vr_mirror::Anchor (0 PLAYSPACE, 1 WORLD)
    uint32_t npcFade{};      // Update 37.2: npc_fade::now() (mode | why << 8)
    uint32_t slotTest{};     // Update 37.2: SlotTest::pack (0 idle)
    uint32_t webBall{};      // Update 37.2: WEB BALL TEST: 0 idle, else SlotTest::pack | eye bloom mode << 16
    uint32_t color{};        // Update 37.2: COLOR FX on (bit 0) | FILTER << 8 | refused (bit 16)
                             // Update 38.5: | OUTLINES (bit 17) | BANDS << 20 (3-5) | 90s shader failed (bit 24)
    uint32_t combat38[combatControls]{};  // Update 38: each COMBAT setting's value (combat38::optionInfo order)
    uint32_t fovea{};        // Update 37.2: FOVEATED level | fault << 8 | debug (bit 16) | working (bit 17)
    float foveaFps[3]{};     // Update 37.2: this level's HIGH, LOW, AVG FPS (0 none yet)
    float foveaGpuMs{};      // Update 37.2: this level's mean eye GPU ms (0 unknown)
    uint32_t guideOff{}, guideNa{}; // Update 37: the CONTROLS page's marks (controls_guide::masks)
    uint32_t sharedLod{};  // Update 36: shared_lod::pack(mode, why it changed on its own)
    // Update 17: the hologram as it is applied (not as a dragged slider
    // shows it): cm over the light, the text factor, the theme and text
    // colour; and the page's scroll (Menu::scroll).
    float heightCm = 9, textScale = 1;
    int theme{}, textColor{};
    float scroll{};
    // Update 38.5: the control whose subtext scrolls, and by how many lines (Menu).
    Control helpFor{};
    float helpScroll{};
    MusicView music; // Update 19
    // 0.0.73 test 3 HOLO ROSTER (fix 28): the heroes on the hologram (Menu::roster; closed: the page as
    // ever), drawn from rosterData (tests, previews) or else holo_roster::latest.
    holo_roster::State roster{};
    const holo_roster::Data* rosterData{};
    // 0.0.73 test 4: HERO MENU HOLO TILES (Menu::Input::heroTiles): POWERS shows the tiles (closed roster:
    // the grid of the hero played), never the old dropdown list.
    bool heroTiles{};
    // 0.0.73 test 3 PORTRAITS: the heroes' tiles show their portraits - the old popup's (the Roster model,
    // Models/Roster: roster_page::pictures, picture k hero k) - as textured quads into rosterImages (the
    // overlay's textured pass, depth-tested, drawn after the translucent one). rosterPictures: an array of
    // roster_page::pictureCount; null, no sink, a layer under 0 or a bad UV box: the initials disc as before.
    const roster_page::Picture* rosterPictures{};
    std::vector<Vertex>* rosterImages{};
    std::array<float, FpsMeter::history> fpsTrail = [] {
        std::array<float, FpsMeter::history> a{};
        a.fill(std::numeric_limits<float>::quiet_NaN());
        return a;
    }();
};
// The panel: 18 by 22 cm at scale 1, its lower edge `gap` over the light
// (Update 17: View::heightCm, hud_height, 0-25 cm; gap is its default, and
// the height is in cm whatever the scale).
constexpr float panelWidth = .18f, panelHeight = .22f, gap = .09f;
static_assert(viewTop == panelHeight / 2 - .015f);
// The panel's middle and its axes (right, up; it faces `viewer`).
struct Frame {
    Vec3 centre, right, up;
};
Frame place(const View&, Vec3 viewer);
// Update 15: where a controller points on the panel. The ray leaves the
// controller's tip (5 cm out along where the hand points, tracking.hpp's
// webShooter) and meets the panel's plane within `reach`; a tip within
// touchDepth of the plane touches it there instead. u, v in the panel's
// measures (scale 1), whether on the panel or not.
struct Pointer {
    bool hit{};
    float u{}, v{};
    Vec3 tip{}, at{};
};
constexpr float pointReach = .6f, touchDepth = .025f;
Pointer point(const Frame&, float scale, const Pose& grip, unsigned hand);

// Update 15: the panel's buttons and sliders, worked by the other hand's
// trigger. A pull that starts (past Toggle::touch) with the pointer on the
// panel is claimed (claims()) until the trigger is let go (under
// Toggle::rest); one already held as the pointer comes onto the panel stays
// the game's. Past Toggle::pressed on a control presses it: an arrow flips
// the page, RECALIBRATE asks for a calibration, a slider is held and follows
// the pointer along its track until the trigger is let go (under
// Toggle::released), on the panel or off it. The AA button switches its
// value between 0 and 1.
//
// Update 17: each page keeps its scroll. The stick (the pointing hand's, up
// and down) scrolls while the pointer is on the panel and nothing is held,
// and is the panel's then (Result::stick: the game must not see it). A pull
// on the scrollbar drags its thumb; one on empty panel drags the content.
// The update the layout changes (Input::layout differs from the last: the
// scale, height, text size or page) hovers nothing and ignores a press.
class Menu {
  public:
    static constexpr float stickDead = .25f, stickSpeed = .16f; // panel metres a second at full tilt
    struct Input {
        bool active{};   // the panel shown whole and in play, the trigger not the toggle's
        Pointer pointer; // the other hand's
        float trigger{}; // the other hand's, 0 untracked
        uint32_t disabled{}; // Applier::disabled: such a control is not pressed or dragged
        bool shadowDistanceOff{}; // Update 33: SHADOW DISTANCE locked (past the mask)
        float stick{};    // Update 17: the pointing hand's stick, up positive
        float seconds{};  // since the previous update
        float text = 1;   // the applied text factor (View::textScale)
        uint32_t layout{}; // anything that sizes or moves the panel, as one key
        // Update 19: the songs in the list, and the one playing or picked (-1 none).
        int musicTracks{}, musicTrack = -1;
        // Update 38.5: how many lines past its rows `helpControl`'s subtext runs
        // as it is drawn now (helpOverflow on the drawing side); -1: work it out
        // from the plain help.
        Control helpControl{};
        int helpOverflow = -1;
        int hero = -1; // Update 39.0.2: hero_modes::Hero being played (POWERS opens its profile); -1 unknown
        // 0.0.73 test 3 HOLO ROSTER (fix 28): HERO MENU CLASSIC (combat38::heroMenu 1): the ROSTER row
        // asks for the old big popup (Result::heroRoster) instead of opening the heroes on the hologram.
        bool heroMenuClassic{};
        // 0.0.73 test 4: HERO MENU HOLO TILES (the game: !heroMenuClassic): POWERS is the tiles - the
        // roster opens on it and closes off it - and the old dropdown list is never shown.
        bool heroTiles{};
        // The roster's data (tests); null: holo_roster::latest, as the game thread published it.
        const holo_roster::Data* rosterData{};
    };
    // Update 19: what the MUSIC page asks of the player this update.
    enum class Music : uint8_t { none, rescan, pick, previous, rewind, toggle, stop, forward, next, seek };
    struct Result {
        bool pressed{}, recalibrate{}, hovered{};
        bool fpsReset{}; // Update 24: RESET pressed (FpsSession::reset)
        bool addressFinder{}; // Update 27: ADDRESS FINDER pressed (start/stop)
        bool eyeLod{};        // Update 28: EYE LOD pressed (on/off)
        bool shadowCamera{};  // Update 31: SHADOW CAMERA pressed (next mode)
        bool shadowDistance{}; // Update 33: SHADOW DISTANCE moved (Settings::shadowDistance)
        bool eyeAa{};          // Update 33: EYE AA pressed (on/off)
        bool engineTaa{};      // Update 34: ENGINE TAA pressed (on/off)
        bool eyeTrail{};       // Update 34.2: EYE AA TRAIL moved (Settings::eyeTrail)
        bool taaJitter{};      // Update 34.2: TAA JITTER pressed (next mode)
        bool farShadows{};     // Update 35: FAR SHADOWS pressed (next mode)
        bool eyeBloom{};       // Update 36: EYE BLOOM pressed (next mode)
        bool sharedLod{};      // Update 36: SHARED LOD pressed (next mode)
        bool mirror{}, mirrorRecentre{}; // Update 37: MIRROR pressed (show/dismiss), RECENTRE HERE
        bool worldLog{};                 // Update 37: WORLD LOG pressed (on/off)
        bool blood{};                    // Update 37: BLOOD SPLATTER pressed (the next step)
        bool mirrorAnchor{}, npcFade{}, slotTest{}, webBallTest{}; // Update 37.2: pressed
        bool colorFx{}, colorFilter{}, colorSlider{};               // Update 37.2: COLOR pressed / moved
        bool colorOutlines{}, colorBands{};                         // Update 38.5: 90s ANIMATED's presses
        bool bloodVisor{};                                          // Update 38.5: VISOR DROPS pressed
        bool invincible{}, flyMode{}, flyVignette{}, symbiote{};    // Update 38.5 EXTRAS: pressed
        bool flyRoll{}, flyRollSpeed{}, flyAutoLevel{};             // Update 38.5: FLY ROLL pressed, ROLL SPEED moved
        bool padMode{}, promptHandling{}, playOnMonitor{};
        bool gestureMoves{};                                        // Update 38.5: GESTURE MOVES pressed
        bool trainMove{};                                           // Version 0.0.2: TRAIN MOVE pressed
        bool heroRoster{};  // Build 8: ROSTER: HEROES AND SUITS pressed (open the roster page) - HERO MENU CLASSIC only
        // 0.0.73 test 3 HOLO ROSTER (fix 28): the heroes on the hologram asked for these (-1 none), the same
        // as the popup's PLAY AS and suit picks; rosterClosed: its CLOSE pressed.
        int rosterPlayAs = -1, rosterSuitHero = -1, rosterSuitIndex = -1;
        bool rosterClosed{};
        bool heroSpider{};  // Update 39.0.1: PLAY AS SPIDER-MAN pressed (every hero toggle off)
        bool section{};     // Update 39.0.2: a section header pressed (opened or closed)
        bool remapMove{}, remapRecord{}, remapKeep{}, remapAgain{}, remapCancel{}; // Update 38.5 GESTURE REMAP
        bool remapResetMove{}, remapResetAll{}, remapProfile{};
        bool foveated{}, foveaInner{}, foveaDebug{}, foveaStats{};  // Update 37.2: FOVEATED's
        int combat38 = -1; // Update 38: the COMBAT row pressed (combat38::Option; -1 none)
        uint32_t changed{}; // a bit (1 << Control) per setting changed in `Settings`
        bool stick{};       // Update 17: the stick is the panel's this update
        bool scrolled{};
        Music music{};
        float musicValue{}; // pick: the song (from 0); seek: where (0..1)
    };
    Result update(const Input&, Settings&);
    int page() const {
        return page_;
    }
    // Update 39.0.2: a page past the eight (an older layout's) is STATUS.
    void setPage(int page) {
        page_ = page >= 0 && page < pages ? page : 0;
    }
    Control hover() const {
        return hover_;
    }
    Control held() const {
        return held_;
    }
    // Update 38.5: the control whose subtext is being read, and its scroll (lines).
    Control helpFor() const {
        return helpFor_;
    }
    float helpScroll() const {
        return helpScroll_;
    }
    static constexpr float helpStickLines = 3.5f, helpAutoLines = 2.f, helpAutoHold = 1.2f, helpBackLines = 4.f;
    bool claims() const {
        return claim_;
    }
    // 0.0.73 test 3 HOLO ROSTER (fix 28): the heroes on the hologram, open in place of the page.
    bool rosterOpen() const {
        return roster_.isOpen();
    }
    const holo_roster::State& roster() const {
        return roster_.state();
    }
    void closeRoster() {
        roster_.close();
    }
    // Update 17: the page's scroll (panel metres, 0 the top).
    float scroll() const {
        return scroll_[static_cast<size_t>(page_)];
    }
    void setScroll(int page, float scroll, float text = 1);
    bool dragging() const {
        return drag_ != Drag::none;
    }
    // Update 19: the song list under the dropdown: open or not, its first
    // row shown, the song pointed at (-1 none).
    bool listOpen() const {
        return listOpen_;
    }
    int listFirst() const {
        return listFirst_;
    }
    int listHover() const {
        return listHover_;
    }
    void closeList() {
        listOpen_ = false;
        listHover_ = -1;
    }

  private:
    enum class Drag : uint8_t { none, bar, content, help };
    Control helpFor_{};
    float helpScroll_{}, helpIdle_{}, helpHold_{};
    bool helpManual_{}, helpBack_{};
    int page_{};
    Control hover_{}, held_{};
    bool claim_{}, touched_{}, down_{};
    std::array<float, pages> scroll_{};
    Drag drag_{};
    float dragV_{}, dragScroll_{};
    uint32_t layout_{};
    bool layoutSeen_{};
    bool listOpen_{};
    int listFirst_{}, listHover_ = -1;
    float listScroll_{}; // rows, fractional while the stick moves it
    float seekShown_ = -1;
    holo_roster::Holo roster_{};
    holo_roster::Data rosterData_{};
    uint32_t rosterVersion_{};
};

// Appends the light, its rays and the panel to `out` (the overlay's
// translucent list): nothing when it is hidden.
void appendWristHud(std::vector<Vertex>& out, const View&, Vec3 viewer, float pixelAngle);
// Update 19: one visualizer (music::visualizerName) alone, in a box
// (u0, v0)-(u1, v1) on the panel of `view` (previews and timing tests).
void appendVisualizer(std::vector<Vertex>& out, const View&, Vec3 viewer, float pixelAngle, int kind, float u0,
                      float v0, float u1, float v1);
// Whether vertices [from, end) are fit to draw: finite, within `radius` of
// `near`, and no more than maxVertices of them.
// Update 16: 120000 (was 60000): the explanation lines and the FPS monitor
// took page 3 near 57000 with its notes; the overlay's upload buffer grows
// to fit (d3d12_renderer.cpp), so this only bounds a runaway.
constexpr size_t maxVertices = 120000;
bool sane(const std::vector<Vertex>& out, size_t from, Vec3 near, float radius);
// Update 38.5: a control's subtext on `page` as `view` would draw it now (its
// state's text if it has one): its lines, the rows it shows, and the widest
// line drawn (either font). found false when the page has no such control.
struct HelpInfo {
    bool found{};
    int lines{}, rows{};
    float widest{};
    float height{}, pitch{};
};
HelpInfo helpInfo(const View& view, int page, Control c);
// Update 38.5: the row is greyed (N/A) and a press on it does nothing.
bool greyed(const View& view, Control c);
// How many lines past its rows the subtext runs (0 when it fits).
int helpOverflow(const View& view, int page, Control c);
// The control whose subtext's rows (u, v) is on, at `scroll` and `text`; none off them.
Control helpAt(int page, float u, float v, float scroll = 0, float text = 1);
} // namespace spidy::wrist_hud
