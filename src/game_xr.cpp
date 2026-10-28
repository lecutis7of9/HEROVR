// Bounded first-person OpenXR integration using the game's two native views.
#ifndef SPIDY_TEST_COMMIT
#define SPIDY_TEST_COMMIT "unknown"
#endif
#ifndef SPIDY_TEST_NUMBER
#define SPIDY_TEST_NUMBER "0"
#endif
#include "spidy/thor_grip.hpp"
#include "spidy/mirror_guard.hpp"
#include "spidy/glove_overlay.hpp" // Version 0.0.72 GAME BODY fallback gloves
#include "spidy/baton_grip.hpp"
#include "spidy/gun_grip.hpp"
#include "spidy/grapple072.hpp" // Version 0.0.72 GRAPPLE (every hero who has one)
#include "spidy/shadow_camera.hpp"
#include "spidy/pixel_font.hpp"
#include "spidy/eye_debug.hpp"
#include "spidy/native_appearance.hpp"
#include "spidy/native_body.hpp"
#include "spidy/native_game_body.hpp"
// The input bridge remains a separately verified, separately stoppable module.
#include "spidy/bhaptics_output.hpp"
#include "spidy/body_calibration.hpp"
#include "spidy/body_haptics.hpp"
#include "spidy/calibration_art.hpp"
#include "spidy/calibration_panel.hpp"
#include "spidy/test_notice.hpp"
#include "spidy/roster_page.hpp"
#include "spidy/health_ring.hpp"
#include "spidy/combat_input.hpp"
#include "spidy/hand_crawl.hpp"
#include "spidy/wall_body.hpp"
#include "spidy/health_log.hpp"
#include "spidy/vr_mirror.hpp"
#include "spidy/world_trace.hpp"
#include "spidy/wrist_muzzle.hpp"
#include "spidy/hero_switch.hpp"
#include "spidy/air_watch.hpp"
#include "spidy/npc_fade.hpp"
#include "spidy/blood_splatter.hpp"
#include "spidy/controls_guide.hpp"
#include "spidy/perf37.hpp"
#include "spidy/crawl_stick.hpp"
#include "spidy/eye_resolution.hpp"
#include "spidy/monitor_rate.hpp"
#include "spidy/eye_snapshot.hpp"
#include "spidy/game_bridge_protocol.hpp"
#include "spidy/game_grab.hpp"
#include "spidy/game_menu.hpp"
#include "spidy/game_offers.hpp"
#include "spidy/game_pad.hpp"
#include "spidy/extras385.hpp"
#include "spidy/gestures385.hpp"
#include "spidy/gesture_session.hpp"
#include "spidy/our_ball.hpp"
#include "spidy/training_guide.hpp"
#include "spidy/game_prompts.hpp"
#include "spidy/game_player.hpp"
#include "spidy/game_punch.hpp"
#include "spidy/game_sense.hpp"
#include "spidy/gadget_wheel.hpp"
#include "spidy/phone.hpp"
#include "spidy/phone_music.hpp"
#include "spidy/reentry_guard.hpp"
#include "spidy/repulsor.hpp"
#include "spidy/repulsor_mcu.hpp"
#include "spidy/hero_modes.hpp"
#include "spidy/hero007_play.hpp" // Version 0.0.7 HERO PROFILES
#include "spidy/hero072_play.hpp" // Version 0.0.72 (TEST)
#include "spidy/van072.hpp"       // Version 0.0.72 PUNISHER VAN (EXPERIMENTAL)
#include "spidy/captain.hpp"      // Version 0.0.7 CAPTAIN AMERICA
#include "spidy/cape_glide.hpp"   // Version 0.0.7 BATMAN cape glide and grapple gun
#include "spidy/mesh_model.hpp"   // Version 0.0.7 BATARANG STYLE
#include "spidy/hero_height.hpp" // Version 0.0.5
#include "spidy/sm2_damage.hpp" // Version 0.0.5
#include "spidy/hit_marks.hpp"  // Version 0.0.62 HIT INDICATOR
#include "spidy/bolt_walls.hpp" // Version 0.0.62 BOLT WALLS
#include "spidy/v005.hpp"       // Version 0.0.5
#include "spidy/v006.hpp"       // Version 0.0.6
#include "spidy/sfx.hpp"        // Version 0.0.5 SOUND EFFECTS
#include "spidy/fly_boost.hpp"  // Version 0.0.5 SUPERSONIC
#include "spidy/music_audio.hpp"
#include "spidy/game_shooter.hpp"
#include "spidy/game_steam.hpp"
#include "spidy/game_swing.hpp"
#include "spidy/game_time.hpp"
#include "spidy/game_tracking.hpp"
#include "spidy/game_warp.hpp"
#include "spidy/hand_pose.hpp"
#include "spidy/impact_sound.hpp"
#include "spidy/native_eye_frame.hpp"
#include "spidy/native_eye_gpu.hpp"
#include "spidy/eye_swap.hpp"
#include "spidy/native_address_finder.hpp"
#include "spidy/native_shadow_distance.hpp"
#include "spidy/eye_taa.hpp"
#include "spidy/engine_taa.hpp"
#include "spidy/far_shadows.hpp"
#include "spidy/eye_bloom.hpp"
#include "spidy/eye_color.hpp"
#include "spidy/foveation.hpp"
#include "spidy/shared_lod.hpp"
#include "spidy/native_eye_history.hpp"
#include "spidy/native_hud.hpp"
#include "spidy/native_webs.hpp"
#include "spidy/near_clip.hpp"
#include "spidy/offer_pointing.hpp"
#include "spidy/presentation_gate.hpp"
#include "spidy/recording_fov.hpp"
#include "spidy/slow_motion.hpp"
#include "spidy/vr_settings.hpp"
#include "spidy/web_pull.hpp"
#include "spidy/wrist_hud.hpp"
#include "spidy/xr_layers.hpp"
#include "spidy/xr_session.hpp"
#include "spidy/mirror_view.hpp" // Version 0.0.3
#include "spidy/rope_grab.hpp"
#include "spidy/v003.hpp"
#include "spidy/suit_wear.hpp"
#include "spidy/wall_stick.hpp"
#include "spidy/web_cocoon.hpp"
#include "spidy/version.hpp"
#include <algorithm>
#include <cctype>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>
#include <mmsystem.h>
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2).
#ifndef SM2_PORT_GAMEPLAY
// Controller world rays (spidy_ray_bridge.dll's Havok cast hook) and native
// swinging (spidy_movement_bridge.dll's mover hooks): Spider-Man 2 addresses
// and bytes from SM2VR 0.0.27's bridges. Each start checks its bytes and, on a
// mismatch or any other refusal, that feature switches itself off with a
// console message; VR goes on. 0 keeps both off (the Phase 1 behaviour).
// SM2TODO (still off inside them): web grab of props/thugs, punches, web
// shooter, the game's wall crawl and wall interactions, game-drawn webs.
#define SM2_PORT_GAMEPLAY 1
#endif
#include <shlobj.h>
#include <thread>
#include <wincodec.h> // Version 0.0.72 REAL MODELS: the models' PNG / JPEG textures
using namespace spidy;
struct XrConfig {
    uint32_t magic = 0x53585243, version = 24, bytes = sizeof(XrConfig), pid{};
    // record and mover are no longer used: VR starts with the game, before
    // there is a player, and finds each new player itself (game_player).
    uint64_t base{}, queue{}, bridgeModule{}, rayModule{}, motionModule{}, record{}, mover{};
    uint32_t durationMs = 20000, eyeSize = defaultEyeSize;
    float swingSpeed = 32;
    // bit 0: CPU eye capture; bit 1: Spidy's overlay webs instead of the game's;
    // bit 2: keep the stock camera as the game's active (monitor) view in VR;
    // bit 3: no web grab (webs never catch props or thugs);
    // bit 4: no eye occlusion (each eye draws everything in its view, hidden or not);
    // bit 5: no body (the hero stays hidden in VR, the overlay draws gloves);
    // bit 6: no punching (fists pass through thugs);
    // bit 7: aim markers start hidden (the SPIDY VR tab shows them);
    // bit 8: no webs in open air (a web that meets nothing within reach misses);
    // bit 9: no web shooter (a free hand's trigger shoots nothing);
    // bit 10: no T-pose calibration at the first gameplay of a session without
    // one (the player skipped it before; the SPIDY VR tab still offers it);
    // bit 11: flips, experimental (A and the left stick in the air flip the player; FlipMotion);
    // bit 12: the trigger webs and the grip reels (WEB BUTTON: TRIGGER);
    // bit 13: the view stays upright on walls (STAND ON WALLS: OFF);
    // bit 14: walls are the game's (its wall crawl takes a player who flies
    // into one, as before the swing's own walls);
    // bit 15: no anti-aliasing for the eyes (each eye is its scene as rendered,
    // at the eye's size, as before October 10's eighth build: upscale is not used);
    // bit 16: the game renders its own view on the head in immersive VR too,
    // for the monitor (as before October 10's eighth build; without it the monitor shows the
    // left eye);
    // bit 17: each eye makes its own set of the key light's shadow cascades,
    // as before October 11 (without it both use one set, fitted to a lens
    // covering both: SpidyEyeShadows);
    // bit 18: no targets for screen-space reflections in the eyes;
    // bit 19: the eyes keep the distances their own wide lenses give
    // (--eye-lens-distances; until October 15 it was webs as before October
    // 12: PHYSICAL WEBS off);
    // bit 20: the game's own button prompts are left alone (neither read
    // for the button they ask for nor named with the VR control, and the
    // interact button is the game's Y as before; the pad layer stays);
    // bit 21: arcade webs (WEB STYLE: ARCADE; vr_settings::Values::arcadeWebs)
    // bit 22: no eye AA (-EyeAA off);
    // bit 23: ENGINE TAA prepared (-EngineTaa, Update 34).
    // Update 34.1: bits past knownOptions are dropped (logged), never a refusal: Update 34
    // refused 0x800000 here (the old limit 0x7fffff), so -EngineTaa started no VR (1001).
    uint32_t options{};
    // OpenXR runtime manifest the launcher chose; empty: Virtual Desktop's if
    // installed, else Windows' active runtime.
    wchar_t runtime[260]{};
    // The rest of the VR settings a session starts from (options bits 3, 5-9
    // and 11-13, swingSpeed, weight, hud and flipSpeed give the others):
    // degrees per snap turn (0: none), controller vibration in percent, the
    // game screen's size (0-2), degrees a second of smooth turning (0: the
    // stick snap turns).
    uint32_t snapTurn = 30, haptics = 100, screenSize = 1, smoothTurn{};
    // Eye resolution in percent of the runtime's recommendation, per side
    // (eye_resolution.hpp; with eyeSize set, 100), and the player's weight
    // while webs fly them, in percent of real gravity (40-300).
    uint32_t renderScale = 100, weight = 100;
    // The player's T-pose calibration (body_calibration.hpp): the eye height
    // and arm length their body is sized to, millimetres. Both 0: none yet.
    uint32_t eyeHeightMm{}, armLengthMm{};
    // The game's HUD in the headset: 0 off, 1 small, 2 medium, 3 large
    // (native_hud.hpp); how fast a flip turns the player at full tilt of the
    // left stick, degrees a second (90-480; FlipMotion::speed).
    uint32_t hud = 2, flipSpeed = 180;
    // Percent of the eye's size, per side, the eyes' scene is rendered at
    // (50-100; eye_resolution.hpp): the engine's temporal anti-aliasing brings
    // it to the eye's size. And how often the game's window gets the left eye
    // while the game renders no view of its own: presents a second (1-240), 0
    // every frame (monitor_rate.hpp).
    uint32_t upscale = defaultEyeUpscale, monitorRate = defaultMonitorRate;
    // Version 23: how much depth scenes have on the game screen: 0 none (the
    // game's flat frame), 1 low, 2 medium, 3 high (vr_settings::screenDepths).
    // Version 24, in version 23's spare word: how strongly a bHaptics vest,
    // sleeves and visor play (BODY HAPTICS, percent; 0 off: body_haptics.hpp).
    uint32_t screenDepth = 2, bodyHaptics = 100;
};
static_assert(sizeof(XrConfig) == 664);
// Why the last frame had no gameplay (XrData::gate bits).
enum GateReason : uint32_t {
    gateNoPlayer = 1,     // no save loaded, or a level change in progress
    gateBridge = 2,       // the input bridge is not running
    gateNoCommit = 4,     // no camera commit for 100 ms: paused, loading, a cutscene
    gateOtherCamera = 8,  // the camera is not the player's follow or combat camera
    gateTracking = 16,    // the headset's pose or timing was not usable
};
struct XrData {
    uint32_t magic = 0x53585244, version = 39, bytes = sizeof(XrData), status{};
    int64_t sequence{};
    uint64_t frames{}, tracked{}, submitted{}, dropped{}, leftHands{}, rightHands{}, serial{}, generation{};
    uint32_t nativeKeys{}, error{};
    float head[16]{}, hands[2][16]{};
    char message[256]{};
    uint64_t uniqueSubmitted{}, reusedSubmitted{};
    uint32_t flatScreen{}, toggles{}, eyeWidth{}, eyeHeight{};
    // What the last frame showed (Presentation); B presses that reached the
    // game as its B (the dodge; 0 from builds before it, where this was
    // reserved); and frames that showed the game's presented frame on the
    // screen because gameplay was unavailable.
    uint32_t presentation{}, dodges{};
    uint64_t screenSubmitted{};
    // GateReason bits of the last frame, and the vtable (image offset) of the
    // camera mover that last committed: which camera a scene without VR used.
    uint32_t gate{}, cameraMover{};
    // Players found so far (each loaded save, respawn or character switch),
    // and the current one's actor record.
    uint64_t players{}, playerRecord{};
    // Xbox buttons the VR controllers hold for the game, and the game's reads
    // of that virtual controller.
    uint32_t padButtons{}, padInstalled{};
    uint64_t padReads{};
    // The input bridge's camera commits, and those on the player by an
    // accepted camera. Both rising while the gate says other_camera: two
    // cameras commit each frame and the other one commits last.
    uint64_t cameraCommits{}, playerCommits{};
    // Y presses that reached the game as its Y (interact, web strike; B
    // presses until October 11); whether the aim markers show now; markers
    // drawn so far, one per hand per headset frame that shows one.
    uint32_t interacts{}, aimMarkers{};
    uint64_t markers{};
    // The VR settings now: the SPIDY VR tab in the game's Settings changes
    // them during play. settings bits: 1 web grab,
    // 2 punch, 4 body, 8 webs in open air, 16 web shooter, 32 flips, 64 the
    // trigger webs (WEB BUTTON: TRIGGER; the grip reels), 128 STAND ON WALLS,
    // 256 PHYSICAL WEBS (until October 15), 512 arcade webs (WEB STYLE:
    // ARCADE).
    // Then the changes
    // made in that tab so far, the times the game built its Settings with it,
    // whether its hooks are in, and why it is missing (game_menu: 93xx-94xx
    // not hooked, 95xx not built).
    uint32_t settings{}, snapTurn{}, haptics{}, screenSize{};
    float swingSpeed{};
    uint32_t settingChanges{};
    uint64_t menuTabs{};
    uint32_t menuInstalled{}, menuStatus{};
    // Degrees a second of smooth turning now (0: the stick snap turns).
    uint32_t smoothTurn{};
    // Walls and ceilings the game held the player on (GameTrackingRig): the
    // stretches and frames of play on one, the player's up (its actor's)
    // now, the head's stand-off now (m), and on the last frame on one the
    // head's height over it from the feet, before and after the stand-off.
    uint32_t surfaceEntries{};
    uint64_t surfaceFrames{};
    float heroUp[3]{};
    float standOff{}, surfaceHeight{}, surfaceClearance{};
    // The weight now, percent of real gravity (the swing's gravity while webs
    // fly the player), and the HUD row now (0 off, 1-3 small to large).
    uint32_t weight{}, hud{};
    // The T-pose calibration (body_calibration.hpp): its phase now and what its
    // panel asks for (Phase, Hint), how far the hold is (0-1); the eye height
    // and arm length the body is sized to (mm; 0: the headset's running height
    // and the hero's own arms); calibrations finished and skipped this session;
    // calibrationFlags bit 0: a session without one asks for none at its first
    // gameplay (skipped); each arm's reach at the last one (m, 0 the left).
    uint32_t calibrationPhase{}, calibrationHint{};
    float calibrationProgress{};
    uint32_t eyeHeightMm{}, armLengthMm{}, calibrations{}, calibrationSkips{}, calibrationFlags{};
    float calibrationReach[2]{};
    // The flip speed now, degrees a second at full tilt (FLIP SPEED), and how
    // far the view leans from the world's up now, degrees: 0 level, 90 while
    // the player stands on a wall (GameMotionFrame::viewTilt).
    uint32_t flipSpeed{};
    float viewTilt{};
    // Version 21: the walks a held Y began (game_pad::InteractStep). Version
    // 22: the GAME SCREEN 3D row now (0 off, 1-3 low to high), and the frames
    // that showed the game screen in stereo (a scene's own two eye images;
    // screenSubmitted counts them too).
    uint32_t interactSteps{}, screenDepth{};
    uint64_t stereoSubmitted{};
    // Version 23: pushes of the right stick that reached the game as its
    // D-pad down (heal) and as its LT + RT (the zip to a point).
    uint32_t heals{}, zips{};
    // Version 24: the game's own camera against where the head looks
    // (game_pad::CameraLook): the angle between them now, degrees (the head to
    // the camera's right: more than 0; 0 while the camera is not steered), and
    // the times the right stick began to turn the camera.
    float lookError{};
    uint32_t lookTurns{};
    // Version 25: the scans a trigger pulled at the temple began (the game's
    // R3: game_pad::TemplePress), and the Charge Jumps a held A began (its
    // RT + A: game_pad::ChargeJump).
    uint32_t scans{}, chargeJumps{};
    // Version 26: the layout the runtime uses for each hand's controller now
    // (ControllerKind, controller_layout.hpp; left, right: 0 none yet,
    // 1 Touch, 2 Index, 3 Steam Frame, 4 one of the runtime's own), the
    // layouts the runtime took Spidy's controls for (bit 1 << ControllerKind)
    // and a spare word. A Steam Frame whose hands say Touch plays SteamVR's
    // translation of that layout: its B, X and Y are all B. Version 38 in
    // that spare word: the zips sent as a hold of the stick was let go in the
    // game's aim mode (game_pad::ZipPress; `zips` counts them too).
    uint32_t controllers[2]{}, controllerLayouts{}, zipReleases{};
    // Version 27: the game's own prompts, read through game_menu's hooks
    // (game_prompts.hpp). Its calls into Flash and the prompt and QTE
    // interface's among them (`promptCalls` at 0 with `promptInvokes` rising
    // says the names no longer match the game), prompts raised and taken
    // away, texts that named a button and texts rewritten for a VR player;
    // what the game is asking for now and the last thing it asked for
    // (prompt_buttons::pack), the asks VR play could not have sent on its own
    // (what the interact button now carries) and texts that came with the
    // keyboard's icons, which name no pad button at all.
    uint64_t promptInvokes{}, promptCalls{}, promptsRaised{}, promptsCleared{};
    uint64_t promptTexts{}, promptRewrites{};
    uint32_t promptDemand{}, promptLastDemand{}, promptBeyondPlay{}, promptKeyboardTexts{};
    // Presses that went to a button the game named, from the VR control of
    // that name or a thrown fist (game_pad::NamedPress, since October 20;
    // before it the interact button sending a prompt's demand, which never
    // ran with the reading off), the times the pad layer was latched on and
    // whether it is on now (game_pad::PadLayer), and a spare word.
    // Then the presses of Y that went out while the player stood on a wall,
    // which `playing` withheld until October 13 (the closed vent).
    uint32_t contextPresses{}, padEntries{}, padLayerNow{}, wallInteracts{};
    // Version 28: the VR controllers' own buttons as the runtime gives them
    // (ControllerButton bits: 1 A, 2 B, 4 X, 8 Y, 16 menu, 32 the Steam
    // Frame's View, 64-512 its D-pad) - held on the last frame, and every bit
    // that went down at all in VR play. A report then says what the player
    // pressed as well as what reached the game, which is what two October 13
    // sessions could not tell apart: the game's Y never reached it in either
    // (0 of 4230 and 0 of 3432 samples) while its B, from the other hand,
    // reached it 30 times, and nothing said whether Y had been pressed.
    uint32_t vrButtons{}, vrButtonsPlayed{};
    // Version 29: presses of the interact that handed the player back to the
    // game's own wall crawl, because the swing held him on a wall of its own
    // and the game was offering nothing to press (game_pad::WallHandover).
    // Then those of them the game answered by offering an interact again,
    // which is the hand-over having worked: the two apart say whether it is
    // worth its moment.
    uint32_t wallHandovers{}, wallHandoversOffered{};
    // Version 37 in version 30's slots (the room around the player, which
    // decided nothing since October 15): the eyes held out of what the world
    // is built of (HeadClearance) - frames they were moved more than a
    // centimetre, and the most they ever were, millimetres.
    uint32_t headPushFrames{}, headPushMostMm{};
    // Version 31: the game's own wall crawl, now that the view turns onto it
    // as it does onto a wall of the swing's (SurfaceHold::crawl). Frames the
    // crawl held the player, and of those the frames the view had turned.
    //
    // Then what the game did with the stick there (CrawlStick): the mean turn
    // from the direction sent to the direction taken, in the surface's own
    // axes, and how alike those turns were - near 1 a frame fixed against the
    // surface, which the stick wants rotating by that turn; near 0 no fixed
    // frame at all, and the mover wants driving instead, as the swing drives
    // it on a wall of its own. Then the mean speed taken. The crawl is the
    // only state the game calls supported on a wall, so it is the only one
    // whose interacts work there: what it does with the stick is the last
    // thing in the way of a vent that behaves like the ground.
    uint32_t crawlFrames{}, crawlStood{}, crawlStickSamples{};
    float crawlStickTurn{}, crawlStickAgreement{}, crawlStickSpeed{};
    // Frames the head was brought down to keep it under the surface over the
    // player, and the least room there ever was over his feet (metres; zero
    // where no ray ever found one). Inside the vent the head stood at the
    // player's own eye height over its floor and so out through its roof.
    uint32_t headDroppedFrames{};
    float headroomLeast{};
    // Version 32: handing the player from the swing's own wall to the game's,
    // indoors (WallToGame). Tries, those the game answered by taking him -
    // which is a vent that works - those it did not and the swing took the
    // wall straight back, and the frames he was being driven into the wall.
    // The three apart say whether the game catches a player let go of at a
    // wall, which nothing could rig headlessly.
    uint32_t wallToGameTries{}, wallToGameHanded{}, wallToGameRecovered{}, wallToGamePushFrames{};
    // Version 33: the game's own crawl under the player at rest on the
    // swing's wall (game_warp): crawls asked for, those the game's state
    // machine took, refused, faulted; how the hand-overs ended - the crawl
    // took him, or the swing had him back (the game did not begin or finish,
    // or the body dropped). Then frames of the game's own wall run (its RT
    // with the stick up the wall, `climbing`) and frames the view held its
    // surface through a gap in the swing's reports (`holdGrace`).
    uint32_t crawlAsked{}, crawlApplied{}, crawlRefused{}, crawlFaulted{};
    uint32_t crawlTaken{}, crawlDeclined{}, gameRunFrames{}, holdGraceFrames{};
    // Version 34: frames of play the rig could not place (the gate's
    // `tracking`), and every reason seen for them (GameTrackingRig::Inactive
    // bits); one shorter than blipSeconds keeps the view standing where it
    // stood. Then frames the game crawled the player on a floor (a vent, a
    // duct: game_swing::Surface::floor), of those the frames the head was
    // held at the crawl's own height (crawlRoom), and frames the body knelt
    // under a held head. Then the game camera's look and up at the latest
    // frame: the frame the game's crawl takes its stick in.
    uint32_t trackingGapFrames{}, trackingGapReasons{}, floorCrawlFrames{}, crawlCapFrames{};
    // Version 35 in the slot after: the game_offers hooks (game_offers::Data
    // status - 0 off, 1 hooked, 3 failed - and its error above the low byte).
    uint32_t kneelFrames{}, offersState{};
    float cameraForward[3]{}, cameraUp[3]{};
    // Version 35: the game's interactions on Spidy's wall (game_offers,
    // offer_pointing): frames a hand pointed at one the game would let the
    // player use, and at one he had to come closer to; presses of the
    // interact that asked for a use, those the game refused at once, those
    // that came to the game's interact and of those the ones the game had
    // picked the offer for first, and those given up (its crawl did not take
    // him); the most offers the game gathered near him at once.
    uint32_t offerPointFrames{}, offerReachFrames{}, offerAsks{}, offerRefusals{};
    uint32_t offerPresses{}, offerPicked{}, offerFailures{}, offersSeen{};
    // Version 36: game_offers' own counts at the latest frame - the player's
    // offer updates run with the crawl's mask, the offers checked against the
    // crawl's pose, the masked updates whose gather checked any offer and the
    // most it checked at once, the spot searches for an aimed offer out of
    // reach and those that found one, and the offers near the player now.
    uint32_t offersMasked{}, offersPosed{}, offersGathered{}, offersMostChecked{};
    // And the presses made again because the game still held the offer.
    uint32_t offersSearches{}, offersSpots{}, offersNear{}, offerRetries{};
    // Version 39: Spider-Sense in the headset (spider_sense.hpp). Warnings
    // that showed (an attacker's mark from its start), headset frames that
    // drew a mark over an attacker and frames that drew one in the sightline
    // for an attacker out of view, tingles sent to the controllers, marks a
    // counter broke, and the game's dodge (B) pressed while a warning stood.
    uint32_t senseShown{}, senseFrames{}, senseSightFrames{}, senseTingles{}, senseBroken{}, senseDodges{};
};
static_assert(sizeof(XrData) == 1176);
// The game's own prompt and QTE calls, for one play session to say what their
// arguments hold (game_prompts.hpp). The eight names matched the game on
// October 13 - 107 of 630 calls into Flash in a 224 s session - but no prompt
// text reached the localized lookup (`texts` 0), so the text is not where a
// session can read what the game is asking for, and these arguments are.
struct PromptData {
    uint32_t magic = 0x53505244, version = 4, bytes = sizeof(PromptData), status{};
    int64_t sequence{};
    uint64_t invokes{}, calls{}, raised{}, cleared{}, texts{}, rewrites{};
    uint32_t demand{}, lastDemand{}, beyondPlay{}, keyboardTexts{};
    // The newest of those calls, one a line with its arguments, oldest first;
    // how many lines are filled; and how often each of the eight names came,
    // in the order of game_prompts.cpp's table (ShowPlaceablePrompt,
    // HidePlaceablePrompt, SetPlaceablePromptProgress, UpdateInWorldPrompt,
    // POIInteractAvailable, AddQTE, UpdateQTE, ClearQTE, ShowText, HideText).
    uint32_t lines{}, reserved{}, perName[game_prompts::callNames]{};
    char line[game_prompts::callLines][game_prompts::callChars]{};
    // Then the census: every distinct name the game called, in the order
    // first seen, how often each came, and one call of each with its
    // arguments. This is what names the call a prompt's text and its button
    // arrive in, without anyone having to guess which it is.
    uint32_t names{}, spare{}, censusCalls[game_prompts::censusNames]{};
    char example[game_prompts::censusNames][game_prompts::censusChars]{};
};
static_assert(sizeof(PromptData) == 12112);
// Slow motion (slow_motion.hpp, game_time.hpp): what the player did with it
// and what the game's time did. status: 1 before VR starts, then 0 with the
// game's time hooked, else why it is not (game_time::install) and slow
// motion does nothing.
struct SlowMotionData {
    uint32_t magic = 0x574f4c53, version = 2, bytes = sizeof(SlowMotionData), status = 1;
    int64_t sequence{};
    // Presses that started it, ended it, or came with too little focus; the
    // times focus ran out and play stopped it (a menu, the game screen, the
    // flat screen); whether it is on now.
    uint32_t starts{}, stops{}, refused{}, emptied{}, interrupted{}, active{};
    // Real seconds in it so far; focus (0-1) and how far into it (0-1) now;
    // the time scale asked of the game.
    float seconds{}, focus = 1, blend{}, wanted = 1;
    // The game's side in the last update of its time system: its own time
    // scale, its world's (the smaller of that and Spidy's), Havok's step (s)
    // and whether the game scales physics itself; its updates so far, and
    // those Spidy slowed.
    float gameScale = 1, worldScale = 1, physicsStep{};
    uint32_t physicsScaled{};
    uint64_t updates{}, slowedUpdates{};
    // Version 2, moments (slow_motion.hpp's SlowMoments): those begun so far
    // by kind (a heavy blow, a called blow, a thug pulled in the air, a
    // perfect dodge), whether one is on now, and the time scale it asks for.
    uint32_t moments[4]{};
    uint32_t momentActive{};
    float momentScale = 1;
};
static_assert(sizeof(SlowMotionData) == 120);
// Body haptics on bHaptics devices (body_haptics.hpp, bhaptics_output.hpp):
// what the bHaptics Player and its devices did. status: bhaptics::Status (0
// off: BODY HAPTICS has not been on; 4 no_player: no bHaptics Player installed,
// as for most players). level: BODY HAPTICS now, percent. Devices on now and
// at any time (bit 1 << body_haptics::Device: 1 the vest, 2 and 4 the left and
// right sleeve, 8 the visor) and the times the Player was connected. Events
// posted, frames sent to a device and those the SDK did not send. Blows the
// player took (a fall in his health) and of them those placed by the attacker
// the game had warned of, and landings. Version 2: ticks of VR play with the
// Player connected (25 a second), and of them each device's with something
// playing and the sum of its strongest motor over those (percent) - how much
// of play a device was on, and how strongly. Then the events by Kind (posted,
// whether their group played them or not), and the groups the tab's switches
// leave on (bit 1 << body_haptics::Group: 1 webs, 2 wind, 4 combat, 8 movement).
struct HapticsData {
    uint32_t magic = 0x53485044, version = 2, bytes = sizeof(HapticsData), status{};
    int64_t sequence{};
    uint32_t level{}, devices{}, devicesSeen{}, connects{};
    uint64_t events{}, frames{}, refused{};
    uint32_t hits{}, aimedHits{}, landings{}, liveTicks{};
    uint32_t onTicks[body_haptics::deviceCount]{}, peakSum[body_haptics::deviceCount]{};
    uint32_t kinds[body_haptics::kindCount]{};
    uint32_t groups{};
};
static_assert(sizeof(HapticsData) == 116 + 4 * body_haptics::kindCount && body_haptics::kindCount == 35);
// Update 20: the wrist hologram's gesture and fallback, and the MUSIC
// player's state, for the session's report (run_game_vr.py
// wrist_hud_snapshot): the October 23 report could not say why the
// hologram never opened, nor whether the music player started. Written by
// the XR thread each frame between two increments of `sequence`.
struct WristHudTelemetry {
    uint32_t magic = 0x53574844, version = 1, bytes = sizeof(WristHudTelemetry), state{};
    int64_t sequence{};
    float distance = -1, nearest = -1, pressDistance = -1, opacity{};
    uint32_t presses{}, pressesInReach{}, pressesOutOfReach{}, pressesNotPlaying{}, pressesRearming{};
    uint32_t toggles{}, keyToggles{}, keyPresses{}, keyRefused{}, blocked{};
    uint32_t musicTracks{};
    int32_t musicTrack = -1;
    uint32_t musicState{}; // 1 available, 2 playing, 4 paused, 8 the status read at least once
    char musicNote[100]{};
};
static_assert(sizeof(WristHudTelemetry) == 192);
// WristHudTelemetry::state and ::blocked (the state at the last pull the
// hologram did not take because the controllers did not play it).
enum : uint32_t {
    hudSettingOn = 1, hudFaulted = 2, hudMenuFaulted = 4, hudPlaying = 8, hudOpen = 16, hudGestures = 32,
    hudGameplay = 64, hudFlatScreen = 128, hudPadLayer = 256, hudPhone = 512, hudWheel = 1024,
    hudCalibrating = 2048, hudHandsTracked = 4096, hudViewing = 8192,
};
extern "C" {
__declspec(dllexport) XrData SpidyXrData;
// Update 37 (tools/update37_records.py).
__declspec(dllexport) combat_input::Data SpidyCombatData;
__declspec(dllexport) hand_crawl::Data SpidyHandCrawlData;
__declspec(dllexport) health_log::Data SpidyHealthData;
__declspec(dllexport) vr_mirror::Data SpidyMirrorData; // Update 37: the mirror (MIRR)
__declspec(dllexport) blood_splatter::Data SpidyBloodData; // Update 37: BLOOD SPLATTER (BLDS)
__declspec(dllexport) perf37::Data SpidyPerfData; // Update 37: the performance pass (PRF7)
__declspec(dllexport) eye_color::Data SpidyColorData;  // Update 37.2: COLOR / ATMOSPHERE (COLR)
__declspec(dllexport) foveation::Data SpidyFoveaData;  // Update 37.2: FOVEATED (FOVE)
__declspec(dllexport) spidy::combat38::Data SpidyCombat38Data; // Update 38: the COMBAT page (CMB8)
__declspec(dllexport) spidy::hero_switch::Data SpidyHeroSwitchData; // Update 38.5: HERO SWITCH PROBE (HSWP)
__declspec(dllexport) spidy::air_watch::Data SpidyAirWatchData;     // Update 38.5: Y IN THE AIR (AIRW)
__declspec(dllexport) spidy::v003::Data SpidyV003Data;               // Version 0.0.3 (V003)
__declspec(dllexport) spidy::v005::Data SpidyV005Data;               // Version 0.0.5 (V005)
__declspec(dllexport) spidy::v006::Data SpidyV006Data;               // Version 0.0.6 (V006)
extern world_trace::Data SpidyWorldTraceData;       // Update 37: the WORLD LOG (world_trace.cpp)
__declspec(dllexport) PromptData SpidyPromptData;
__declspec(dllexport) SlowMotionData SpidySlowMotionData;
__declspec(dllexport) HapticsData SpidyHapticsData;
__declspec(dllexport) WristHudTelemetry SpidyWristHudData;
__declspec(dllexport) XrTimingData SpidyXrTimingData;
__declspec(dllexport) EyeSnapshot SpidyXrSnapshot;
DWORD WINAPI SpidyStart(void*);
DWORD WINAPI SpidyEyeTemporal(void*);
DWORD WINAPI SpidyEngineTaa(void*); // Update 34: ENGINE TAA, 0 off, 1 on, 2 off (slow)
DWORD WINAPI SpidyEyeShadows(void*);
DWORD WINAPI SpidyMonitorRate(void*);
// Update 15: the wrist hologram's GRAPHICS page (native_eye_gpu.cpp,
// stereo_probe.cpp): the same calls live.txt's aa, history and reach make.
DWORD WINAPI SpidyEyeAA(void*);
DWORD WINAPI SpidyEyeHistory(void*);
DWORD WINAPI SpidyEyeSharpen(void*);
DWORD WINAPI SpidyImpostorReach(void*);
uint32_t SpidyImpostorReachNow();
// Update 38.3 FAR WINDOWS (stereo_probe.cpp): the impostor (far building windows) reach as a
// percent of the game's, and its counts (calls, writes, the game's metres, ours).
DWORD WINAPI SpidyFarWindows(void* percent);
// Version 0.0.4 MIRROR VIEW TEST (stereo_probe.cpp): wanted (made only when the eyes are made: a
// restart), and its stats for the V003 record.
void SpidyMirrorViewWant(uint32_t on);
void SpidyMirrorViewRead(spidy::mirror_view::Stats* out);
void SpidyFarWindowsStats(uint64_t* calls, uint64_t* writes, float* gameMetres, float* ourMetres);
uint32_t SpidyEyeLodNow();             // Update 28: EYE LOD, 0 off, 1 on, 2 failed
uint32_t SpidyEyeLodReason();          // Update 28.1: native_view::EyeLodReason once failed
DWORD WINAPI SpidyEyeLod(void* on);     // Update 28
uint32_t SpidyShadowCameraNow();        // Update 31: SHADOW CAMERA mode | reverted reason << 8
uint32_t SpidyEngineTaaNow();           // Update 34: engine_taa::pack(state, reason)
DWORD WINAPI SpidyEngineTaaJitter(void* mode); // Update 34.2: engine_taa::JitterMode, 4 the next
uint32_t SpidyEngineTaaJitterNow();     // Update 34.2
DWORD WINAPI SpidyShadowCamera(void* mode); // Update 31
DWORD WINAPI SpidyFarShadows(void* mode); // Update 35: far_shadows::Mode, 3 the next, 4 off (slow)
uint32_t SpidyFarShadowsNow();            // Update 35: far_shadows::pack(mode, reason)
DWORD WINAPI SpidyEyeBloom(void* mode);  // Update 36: eye_bloom::Mode, 3 the next
uint32_t SpidyEyeBloomNow();             // Update 36: eye_bloom::pack(mode, reason)
DWORD WINAPI SpidySharedLod(void* mode); // Update 36: shared_lod::Mode, 3 the next
uint32_t SpidySharedLodNow();            // Update 36: shared_lod::pack(mode, reason)
// Update 18: the webs' look, below (live.txt web_style, web_color).
__declspec(dllexport) DWORD WINAPI SpidyWebStyle(void*);
__declspec(dllexport) DWORD WINAPI SpidyWebColor(void*);
DWORD WINAPI SpidyStop(void*);
DWORD WINAPI SpidySetEyes(void*);
float SpidyFlatAspect();
}
namespace {
using BridgeCall = DWORD(WINAPI*)(void*);
XrConfig config;
BridgeCall submitInput{}, sampleBridge{}, stopBridge{}, retargetBridge{};
BridgeCall startRays{}, submitRays{}, stopRays{};
// The world rays' results, read where they are written: the bridge exports the
// struct itself, and the session is in the same process as it.
const native_rays::Data* rayResults{};
BridgeCall startSwing{}, submitSwing{}, sampleSwing{}, stopSwing{}, retargetSwing{}, sampleGrab{}, sampleAim{};
// Each web's edges (game_swing::Bends): optional, without it a web round an
// edge is drawn to that edge.
BridgeCall sampleBends{};
BridgeCall sampleClimb{};
BridgeCall startPunch{}, samplePunch{}, stopPunch{}, swingSettings{}, sampleStrike{};
// Update 38: LUNGE SPIDYVR MOVE's lunges (optional: an older ray module has none, and the
// lunge stays the game's web strike, GAME BUTTON).
BridgeCall sampleLunge{};
BridgeCall sampleAirYield{}; // Update 38.5: optional (an older ray module: no yield counts)
BridgeCall sampleFly{};      // Update 38.5 FLY MODE: optional (an older ray module: no flight)
BridgeCall sampleCape{};     // Version 0.0.7 BATMAN: optional (an older ray module: no grapple line drawn)
// Spider-Sense (game_sense, in the same module as the punches): optional, an
// older module draws no warnings.
BridgeCall sampleSense{};
BridgeCall startShooter{}, sampleShooter{}, stopShooter{}, wheelShooter{};
// The game's own crawl under the player (game_warp, in the same module):
// optional, for the session report.
BridgeCall sampleWarp{};
// The game's interactions on Spidy's wall (game_offers, in the same module):
// what it gathered near the player, and the use of the one he points at.
BridgeCall sampleOffers{}, useOffer{}, aimOffer{};
std::mutex lifecycle, telemetry;
HANDLE worker{};
std::atomic<bool> stopRequested{};
std::atomic<uint64_t> keepAliveDeadline{};
// The webs' half width, in tenths of a millimetre (live.txt web_width; WebLook's default).
std::atomic<uint32_t> webRadiusTenthsMm{static_cast<uint32_t>(WebLook{}.radius * 1e4f + .5f)};
// Update 15: the shot's flight as drawn (live.txt web_travel, web_speed m/s).
// Only the picture; the swing attaches when it always did.
std::atomic<bool> webTravelOn{WebTravel{}.travel};
std::atomic<uint32_t> webSpeedMs{static_cast<uint32_t>(WebTravel{}.speed)};
// Update 37: strand physics (live.txt web_physics, web_wiggle, web_bend; web_visual.hpp).
std::atomic<bool> webPhysicsOn{WebTravel{}.physics};
std::atomic<uint32_t> webWiggle{static_cast<uint32_t>(WebTravel{}.wiggle)},
    webBend{static_cast<uint32_t>(WebTravel{}.bend)};
// Update 37: combat by gesture (combat_input.hpp; live.txt combat, tap_ms, gesture_ms, the
// four gadgets' slots, their speeds, punch, aim). Speeds in cm/s, looseness in percent.
std::atomic<bool> combatOn{combat_input::Config{}.on}, punchOn{combat_input::Config{}.punch},
    aimSteerOn{combat_input::Config{}.steer},
    // Update 37.2: ZIP STRIKE (default on) and WEB YANK (default off, a candidate input).
    zipStrikeOn{combat_input::Config{}.zipStrike}, webYankOn{combat_input::Config{}.webYank};
std::atomic<uint32_t> yankSlot{combat_input::Config{}.yankSlot};
// Update 37.2: COLOR / ATMOSPHERE (eye_color.hpp; live.txt color_fx ... vignette), default OFF,
// graded in the headset's sharpen pass (native_eye_gpu). colorDirty: sent again until taken.
std::atomic<bool> colorFxOn{eye_color::Config{}.on}, colorDirty{true}, colorRefused{false};
std::atomic<uint32_t> colorFilter{eye_color::Config{}.filter}, colorStrength{eye_color::Config{}.strength},
    colorContrast{eye_color::Config{}.contrast}, colorSaturation{eye_color::Config{}.saturation},
    colorVibrance{eye_color::Config{}.vibrance}, colorVignette{eye_color::Config{}.vignette};
std::atomic<int32_t> colorExposure{eye_color::Config{}.exposureTenths}, colorWarmth{eye_color::Config{}.warmth},
    colorTint{eye_color::Config{}.tint};
// Update 38.5: 90s ANIMATED's CARTOON STRENGTH, OUTLINES and BANDS (live.txt color_cartoon, color_outlines,
// color_bands); toonNoted: its shader's failure was told (and the FILTER put back to NONE) once.
std::atomic<uint32_t> colorCartoon{eye_color::Config{}.cartoon}, colorBands{eye_color::Config{}.bands};
std::atomic<bool> colorOutlines{eye_color::Config{}.outlines}, toonNoted{false};
uint32_t heroNow();
uint32_t c38(uint32_t option); // Version 0.0.62 BEACONS IN VISION (colorWanted)
eye_color::Config colorWanted() {
    eye_color::Config c;
    c.on = colorFxOn.load();
    // Version 0.0.6 DAREDEVIL SONAR: on with DAREDEVIL ABILITIES, in place of the COLOR page's look.
    if (heroNow() == hero_modes::daredevil) {
        c.on = true;
        c.sonar = true;
        c.sonarBeacons = combat38::beaconsInVision(c38(combat38::beaconsVision)); // Version 0.0.62
    }
    c.filter = colorFilter.load();
    c.strength = colorStrength.load();
    c.exposureTenths = colorExposure.load();
    c.contrast = colorContrast.load();
    c.saturation = colorSaturation.load();
    c.vibrance = colorVibrance.load();
    c.warmth = colorWarmth.load();
    c.tint = colorTint.load();
    c.vignette = colorVignette.load();
    c.cartoon = colorCartoon.load();
    c.outlines = colorOutlines.load();
    c.bands = colorBands.load();
    return c;
}
// Update 37.2: FOVEATED (EXPERIMENTAL, foveation.hpp; live.txt foveated, fovea_inner,
// fovea_debug), default OFF. foveaDirty: set again until the eye copy runs and takes it.
std::atomic<uint32_t> foveaLevelWant{foveation::off}, foveaInnerWant{foveation::defaultInner};
std::atomic<bool> foveaDebugWant{false}, foveaDirty{false}, foveaResetAsked{false};
foveation::Perf foveaPerf; // the XR thread's only
std::atomic<uint32_t> combatTapMs{combat_input::Config{}.tapMs}, combatGestureMs{combat_input::Config{}.gestureMs},
    grabberSlot{combat_input::Config{}.grabberSlot}, sonicSlot{combat_input::Config{}.sonicSlot},
    upshotSlot{combat_input::Config{}.upshotSlot}, ricochetSlot{combat_input::Config{}.ricochetSlot},
    yankCms{160}, upshotCms{160}, upshotLowDegrees{15}, flickCms{180}, sonicGapCm{25}, sonicReachCm{30},
    punchCms{static_cast<uint32_t>(combat_input::Config{}.punchSpeed * 100.f + .5f)}, aimConeDegrees{18}, lockStickDegrees{12}, loosePercent{130};
// Update 38: the COMBAT page's live settings (combat38.hpp; live.txt and the hologram, each at
// once, no restart), index 0 of each the default.
std::atomic<uint32_t> combat38Values[combat38::options]{};
// Version 0.0.72 MODEL MISSING: one bit per mesh_model slot whose MODEL setting is on and whose model
// could not be read (the render thread sets it; the wrist names them: "MODEL MISSING: <name>").
std::atomic<uint32_t> modelMissingMask{0};
std::string modelMissingNote(uint32_t mask) {
    std::string t;
    for (uint32_t s = 0; s < mesh_model::slots && s < 32; ++s) // build 9: 42 slots, the mask's 32 bits
        if (mask >> s & 1u)
            t += (t.empty() ? "MODEL MISSING: " : ", ") + std::string(mesh_model::slotFolders[s]);
    return t;
}
// Build 9 SUITS (suit_wear.hpp): the active hero's picked suit's mesh_model slot (0: none - DEFAULT, the
// game's own or OWN SUIT; the game thread sets it each step), each suit's rig (made once at load, read by
// the render thread after) and the wear's cost (the render thread's mean, microseconds a frame).
std::atomic<uint32_t> wornSuitSlot{0};
suit_wear::Rig suitRigs[mesh_model::suitSlots];
std::atomic<uint32_t> suitWearMicros{0}, suitWearVertices{0};
// Test build 10 ONE BODY: when the render thread last drew the worn suit's SUIT BODY part (GetTickCount64; 0 never):
// the game's own body and the fallback gloves are off while it is fresh (suit_wear::shownNow).
std::atomic<uint64_t> suitShownAtMs{0};
// Test build 12 CAPE: BATMAN's cape glide on (the game thread, from the bridge's cape_glide::Data): the render
// thread fans the worn suit's cape out into wings while it is (suit_wear::cape).
std::atomic<bool> capeGlidingNow{false};
std::atomic<bool> suitsRigged{false}; // Test build 10: suitRigs made (the game thread reads them after)
bool suitShownNow() { return suit_wear::shownNow(GetTickCount64(), suitShownAtMs.load(std::memory_order_relaxed)); }
// Version 0.0.72 REAL MODELS: a model texture (PNG, JPEG, anything Windows reads) to RGBA8 with WIC, its
// sRGB values kept; false (that image left untextured) on any failure. COM is set up by the caller.
bool wicDecode(const uint8_t* data, size_t size, mesh_model::Picture& out) {
    if (!data || !size || size > mesh_model::mostImageBytes)
        return false;
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    Microsoft::WRL::ComPtr<IWICStream> stream;
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
        FAILED(factory->CreateStream(&stream)) ||
        FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(data), static_cast<DWORD>(size))) ||
        FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) ||
        FAILED(decoder->GetFrame(0, &frame)) || FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                                     WICBitmapPaletteTypeCustom)))
        return false;
    UINT w = 0, h = 0;
    if (FAILED(converter->GetSize(&w, &h)) || !w || !h || w > 8192 || h > 8192)
        return false;
    out.width = w;
    out.height = h;
    out.rgba.assign(size_t{w} * h * 4, 0);
    return SUCCEEDED(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(out.rgba.size()), out.rgba.data()));
}
std::string modelsRoot() {
    const std::wstring home = spidyHome() + L"\\Models\\";
    std::string root(static_cast<size_t>(WideCharToMultiByte(CP_UTF8, 0, home.c_str(), -1, nullptr, 0, nullptr, nullptr)), '\0');
    if (!root.empty()) {
        WideCharToMultiByte(CP_UTF8, 0, home.c_str(), -1, root.data(), static_cast<int>(root.size()), nullptr, nullptr);
        root.resize(root.size() - 1);
    }
    return root;
}
std::atomic<float> heroHeightNow{1.f}; // Version 0.0.5 HERO HEIGHT in force (the V005 record)
uint32_t c38(uint32_t option) {
    // Version 0.0.2 hotfix: a getter that reaches itself again answers the default (0) rather than recursing.
    static thread_local int depth = 0;
    const ReentryGuard guard{depth};
    if (guard.reentered())
        return 0u;
    return option < combat38::options ? combat38::clampValue(option, combat38Values[option].load()) : 0u;
}
// Version 0.0.4 HERO ABILITIES (hero_modes.hpp): any hero but SPIDER-MAN has no web path at all.
// Version 0.0.6: IRON MAN (shooterType 1), DAREDEVIL and HULK; only one on (heroOnly).
uint32_t heroNow() {
    return hero_modes::fromToggles(c38(combat38::shooterType) == repulsor::repulsor, c38(combat38::daredevil) == 1,
                                   c38(combat38::hulk) == 1, c38(combat38::batman) == 1, c38(combat38::wolverine) == 1,
                                   c38(combat38::superman) == 1, c38(combat38::punisher) == 1, // Version 0.0.7
                                   c38(combat38::captainAmerica) == 1,                         // CAPTAIN AMERICA
                                   c38(combat38::thor) == 1, c38(combat38::greenLantern) == 1); // Version 0.0.72
}
// Version 0.0.72 (TEST): every 0.0.72 option (hero072_play.hpp PlayOptions) from combat38, live.
hero072::PlayOptions hero072Options() {
    using O = hero072::PlayOptions;
    O o;
    o.thorLightning = O::onFirst(c38(combat38::thorLightning));
    o.thorStorm = O::onFirst(c38(combat38::thorStorm));
    o.thorChain = O::onFirst(c38(combat38::thorChain));
    o.glFlight = O::onFirst(c38(combat38::glFlight));
    o.glFist = O::onFirst(c38(combat38::glFist));
    o.glBeam = O::onFirst(c38(combat38::glBeam));
    o.glBubble = O::onFirst(c38(combat38::glBubble));
    o.glChain = O::onFirst(c38(combat38::glChain));
    o.glCharge = O::onFirst(c38(combat38::glCharge));
    o.spiderClimbOnly = O::onFirst(c38(combat38::spiderClimbOnly));
    o.uppercut = O::onFirst(c38(combat38::uppercut));
    o.smokeBlind = O::onFirst(c38(combat38::smokeBlind));
    o.ddGrapple = O::onFirst(c38(combat38::ddGrapple));
    o.ddParry = O::onFirst(c38(combat38::ddParry));
    o.ironHud = O::onFirst(c38(combat38::ironHud));
    o.punisherRage = O::onFirst(c38(combat38::punisherRage));
    o.reputation = O::onFirst(c38(combat38::reputation));
    o.survival = O::offFirst(c38(combat38::survival));
    o.punisherSuit = c38(combat38::punisherSuit);
    o.naturalInvincible = O::onFirst(c38(combat38::naturalInvincible));
    o.hulkRage = O::onFirst(c38(combat38::hulkRage));
    o.rageBlood = c38(combat38::rageBlood);
    o.rageTint = c38(combat38::rageTint);
    o.fireDouse = O::onFirst(c38(combat38::fireDouse));
    o.shieldSlam = O::onFirst(c38(combat38::shieldSlam));
    o.punisherHook = O::onFirst(c38(combat38::punisherHook));
    o.clawClimb = O::onFirst(c38(combat38::clawClimb));
    o.shove = O::onFirst(c38(combat38::npcShove));
    o.faction = c38(combat38::faction);
    o.testUnlocks = c38(combat38::testUnlocks);
    o.ultimates = O::onFirst(c38(combat38::ultimates));
    o.perkBranch = c38(combat38::perkBranch);
    o.ownSuit = O::offFirst(c38(combat38::ownSuit));
    o.perchTakedown = O::onFirst(c38(combat38::perchTakedown));
    o.takedownLight = O::onFirst(c38(combat38::takedownLight));
    o.punisherVan = O::offFirst(c38(combat38::punisherVan));
    o.thorHammer = std::min<uint32_t>(c38(combat38::thorHammer), mesh_model::hammers - 1);
    return o;
}
// Version 0.0.6: a hero toggle just turned on turns the other two off (every write path: the
// hologram, the live exports, live.txt at start).
// Build 8 ROSTER page: a suit picked on it (tracking thread), taken by the game thread: hero * 16 + suit + 1 (0: none).
std::atomic<uint32_t> rosterSuitRequest{0};
void heroOnly(uint32_t option) {
    if (!combat38::isHeroToggle(option) || combat38Values[option].load() == 0)
        return;
    for (const auto o : combat38::heroToggles)
        if (o != option)
            combat38Values[o] = 0;
}
// Version 0.0.3 AUTO PERFORMANCE: what the render thread's controller did, for the V003 record (the
// game thread publishes it; this lock is the only thing the two threads share for it).
struct PerfReport {
    uint32_t on{}, rung{}, zone{}, eventCount{};
    float target{}, meanFps{}, missed{}, headsetHz{};
    uint32_t userFar{}, userLod{}, userShadows{}, nowFar{}, nowLod{}, nowShadows{}, refused{}, userChanged{};
    uint64_t drops{}, raises{}, zoneJumps{}, bans{}, restores{};
    auto_perf::Event log[v003::perfEvents]{};
};
std::mutex perfReportLock;
PerfReport perfReport;
// Version 0.0.3: the hero's position (world) and speed for AUTO PERFORMANCE's zone (game thread writes).
std::atomic<float> perfHeroX{std::numeric_limits<float>::quiet_NaN()}, perfHeroZ{std::numeric_limits<float>::quiet_NaN()},
    perfHeroSpeed{0.f}, perfEnemyMetres{std::numeric_limits<float>::infinity()};
// Update 38.5 EXTRAS (extras385.hpp), each OFF by default and live; the cheats start OFF each session.
std::atomic<bool> invincibleOn{false}, flyVignetteOn{false}, symbioteOn{false};
std::atomic<uint32_t> flyModeNow{0};       // extras385::fly::Speed
// Update 38.5 FLY ROLL (extras385::fly::RollMode, 1 COMFORT), its speed (degrees a second) and AUTO LEVEL.
std::atomic<uint32_t> flyRollNow{1}, flyRollSpeedNow{90};
std::atomic<bool> flyAutoLevelNow{true};
std::atomic<uint32_t> promptHandlingNow{0}; // extras385::prompts::Handling
std::atomic<uint32_t> gestureMovesNow{0};   // Update 38.5: gestures385::Mode (0 OFF, 1 TRAINING, 2 ON)
static_assert(gestures385::pad::dpadUp == game_pad::dpadUp && gestures385::pad::lb == game_pad::leftShoulder &&
              gestures385::pad::rb == game_pad::rightShoulder && gestures385::pad::a == game_pad::a &&
              gestures385::pad::b == game_pad::b && gestures385::pad::x == game_pad::x &&
              gestures385::pad::y == game_pad::y && gestures385::pad::l3 == game_pad::leftThumb &&
              gestures385::pad::r3 == game_pad::rightThumb && gestures385::pad::dpadDown == game_pad::dpadDown,
              "GESTURE MOVES' buttons are the virtual pad's");
std::atomic<uint32_t> invincibleRefills{0}, invincibleNa{0};
combat_input::Config combatConfig() {
    combat_input::Config c;
    c.on = combatOn.load();
    c.tapMs = combatTapMs.load();
    c.gestureMs = combatGestureMs.load();
    // Update 38.2 GADGET KEYS: 38.2 (Grabber RB+B CONFIRMED in run381; the rest CANDIDATE),
    // GAME DEFAULT, or CUSTOM (live.txt grabber_slot ... ricochet_slot, 38.1's).
    const auto slots = combat38::gadgetSlots(c38(combat38::gadgetKeys),
                                             {grabberSlot.load(), sonicSlot.load(), upshotSlot.load(), ricochetSlot.load()});
    c.grabberSlot = slots.grabber;
    c.sonicSlot = slots.sonic;
    c.upshotSlot = slots.upshot;
    c.ricochetSlot = slots.ricochet;
    c.yankSpeed = static_cast<float>(yankCms.load()) / 100.f;
    c.upSpeed = static_cast<float>(upshotCms.load()) / 100.f;
    c.lowDegrees = static_cast<float>(upshotLowDegrees.load());
    c.flickSpeed = static_cast<float>(flickCms.load()) / 100.f;
    c.blastGap = static_cast<float>(sonicGapCm.load()) / 100.f;
    c.blastReach = static_cast<float>(sonicReachCm.load()) / 100.f;
    c.punch = punchOn.load();
    c.punchSpeed = static_cast<float>(punchCms.load()) / 100.f;
    c.steer = aimSteerOn.load();
    c.zipStrike = zipStrikeOn.load();
    c.webYank = webYankOn.load();
    c.yankSlot = yankSlot.load();
    c.aimCone = static_cast<float>(aimConeDegrees.load());
    c.lockStick = static_cast<float>(lockStickDegrees.load());
    c.loose = static_cast<float>(loosePercent.load()) / 100.f;
    // Update 38: ZIP MODE, PUNCH MODE, BALL AIM.
    c.zipMode = c38(combat38::zipMode);
    // Update 38.2 PUNCH MODE: TARGETED (0, the default) is 38.1's BOTH with an enemy in reach
    // or locked; BOTH / GESTURE / CONTACT are 38.1's 0-2.
    const uint32_t pm = c38(combat38::punchMode);
    c.punchTargeted = pm == 0;
    c.punchMode = pm == 0 ? 0u : pm - 1;
    // Update 38.2 BALL AIM: SMART (0), SPIDYVR, CONE 20, OFF.
    const uint32_t aa = c38(combat38::aimAssist);
    c.assistSmart = aa == combat38::assistSmart;
    c.assistSpidy = aa == combat38::assistSpidyVr;
    c.assistDegrees = combat38::assistCone(aa);
    // Update 38.2 MELEE ANIM PUNCH ONLY and GADGET FEEL EASY (plan B: GAME, 38.1).
    // Version 0.0.4: EVERY HIT (0, one X per blow at once) is the default, SPACED (2) is
    // 0.0.3's PUNCH ONLY; and a fist re-arms as its swing ends or it comes back (rep0030).
    c.meleeEveryHit = c38(combat38::meleeAnim) == 0;
    c.meleePunchOnly = c38(combat38::meleeAnim) == 2;
    c.quickRearm = true;
    c.closeJabs = c.meleeEveryHit; // a jab at 0.85 of PUNCH SPEED with an enemy in reach
    c.gadgetEasy = c38(combat38::gadgetFeel) == 0;
    // Update 38 (SpidyVR fold-in): LUNGE and BALL FIRE.
    c.lungeMode = c38(combat38::lunge);
    // Update 38.5 FLY MODE: a punch at range flies you into the enemy by the bridge's own mover
    // (SPIDYVR MOVE at the zip strike's reach), never the game's Y web strike: no web line.
    if (flyModeNow.load())
        c.lungeMode = combat38::lungeSpidyMove;
    c.ballOnPress = c38(combat38::ballFire) == 0;
    // Version 0.0.2 BALL LOOK OURS: a web-shooter press fires our own ball, no RB to the game.
    c.ballOurs = c38(combat38::ballLook) == combat38::lookOurs;
    // Version 0.0.4 IRON MAN ABILITIES (REPULSOR): the trigger makes the mod's own shot at the press only
    // (no RB, no game ball) and no trigger gadget, yank or Sonic Burst (repulsor.hpp planFor).
    // Version 0.0.6: DAREDEVIL (the trigger throws a baton) and HULK (no trigger power) the same: no web.
    if (heroNow() != hero_modes::spiderMan) {
        c.ballOurs = true;
        c.ballOnPress = true;
        c.noTriggerGadgets = true;
        // Version 0.0.7: no web at all - no web yank, and a punch at range never sends the game's
        // Y web strike: the bridge's own lunge flies him (SPIDYVR MOVE, no web line) or none.
        c.webYank = false;
        c.lungeMode = combat38::lungeSpidyMove;
        c.zipStrike = c.zipStrike && c.zipMode == combat38::zipSpidy;
    }
    // Update 38.1: LUNGE FROM, LUNGE SPEED, LUNGE AIM, PUNCH ANGLE, BALL LEAD.
    c.lungeFrom = combat38::lungeFromMetres(c38(combat38::lungeFrom));
    c.lungeSpeed = combat38::lungeSpeedOf(c38(combat38::lungeSpeed));
    c.lungeSteady = c38(combat38::lungeAim) == 0;
    c.punchForward = combat38::punchForwardOf(c38(combat38::punchAngle));
    // Version 0.0.5 PUNCH SENSITIVITY: FORGIVING (the default) lowers the speed, widens the angle,
    // shortens the reach and re-arms on a partial pull-back (combat38::punchFeelOf).
    combat_input::applyPunchFeel(c, c38(combat38::punchSense));
    c.ballLeadAlways = c38(combat38::ballLead) == 1;
    // Update 38.3: LOOK LOCK QUICK and STRIKE STEER TO LOCK (plan B: STEADY, OFF).
    c.lookQuick = c38(combat38::lookLock) == 0;
    c.strikeSteer = c38(combat38::strikeSteer) == 0;
    // Update 38.5: far punches (run384: the first 2-3 at a distance and the mid-air ones did
    // nothing): the lock's reach out to ZIP FAR, the aim cone wider, re-armed by time too.
    c.farLunge = true;
    // Version 0.0.62 PUNCH HITS INSTANT ONLY: no X for a blow (the instant DirectDamage blow is the
    // hit) - only while the damage path is open and the hero is Spider-Man or Iron Man (DAREDEVIL and
    // HULK keep their X and their own extra blow); a closed path sends the X as 0.0.6.
    c.noPunchX = c38(combat38::punchHits) == combat38::punchHitsInstant && sm2_damage::enabled() &&
                 heroNow() != hero_modes::daredevil && heroNow() != hero_modes::hulk;
    return c;
}
// Update 37: hand-over-hand climbing on the game's crawl (hand_crawl.hpp; live.txt
// hand_climb, climb_pull, climb_grip) and which wall the player is held on (wall_crawl:
// 1 the game's own crawl, 0 the swing's wall run).
// Update 37.2: wall_crawl 0 by default again (Update 36's swing wall run; the game's crawl
// turned the view upright/inverted on walls in run371).
std::atomic<bool> climbOn{hand_crawl::Config{}.on}, gameWallCrawl{false}, wallCrawlChanged{};
// Update 37.2: WALL BODY (wall_body.hpp; live.txt wall_body), default OFF: the drawn body lies
// against a wall instead of standing out from it (only the picture).
std::atomic<bool> wallBodyOn{wall_body::defaultOn};
std::atomic<int> handClimbAsked{-1};
std::atomic<uint32_t> climbPull{hand_crawl::Config{}.pull}, climbGrip{hand_crawl::Config{}.grip};
// Update 37: the right stick down heals (live.txt heal, heal_hold_ms).
std::atomic<bool> healOn{true};
std::atomic<uint32_t> healHoldMs{250};
// Update 37: the ring on the wrist (health_ring::Place; live.txt ring_face, ring_offset,
// ring_tilt): flat on the web shooter (1) or facing the eyes as Update 36 (0).
std::atomic<bool> ringFlat{health_ring::Place{}.flat};
std::atomic<uint32_t> ringOffsetMm{static_cast<uint32_t>(health_ring::Place{}.offsetMm + .5f)};
std::atomic<int> ringTiltDegrees{0};
// Update 37: the MIRROR (vr_mirror.hpp; live.txt mirror, mirror_distance, mirror_size,
// mirror_spot, mirror_anchor, mirror_style; holo page 2 MIRROR and RECENTRE MIRROR).
std::atomic<bool> mirrorOn{vr_mirror::Config{}.on};
std::atomic<uint32_t> mirrorDistanceCm{vr_mirror::Config{}.distanceCm};
std::atomic<uint32_t> mirrorSizePercent{vr_mirror::Config{}.sizePercent};
std::atomic<uint32_t> mirrorSpot{vr_mirror::Config{}.spot};
std::atomic<uint32_t> mirrorAnchor{vr_mirror::Config{}.anchor};
std::atomic<uint32_t> mirrorStyle{vr_mirror::Config{}.style};
std::atomic<bool> mirrorRecentreAsked{false};
// Update 37.2: SLOT TEST and WEB BALL TEST, asked on the hologram, sent from the pad loop
// (1 the next slot input, 2 one RB tap); their state back for the hologram.
std::atomic<uint32_t> testAsked{0};
std::atomic<uint32_t> slotTestPacked{0}, webBallPacked{0};
std::atomic<uint32_t> mirrorMicros{0}; // the drawing's mean cost, for the holo
// Test build 10 MIRROR VIEW (mirror_guard.hpp): the reflection at 45 Hz (the wrist's HALF), and when the
// mirror turned itself OFF (GetTickCount64, 0 never) with its cost then (us; 0: it failed) for the wrist note.
std::atomic<bool> mirrorHalfRate{false};
std::atomic<uint64_t> mirrorAutoOffAtMs{0};
std::atomic<uint32_t> mirrorAutoOffMicros{0};
// Update 37: BLOOD SPLATTER (blood_splatter.hpp), intensity percent, 0 OFF (the default).
std::atomic<uint32_t> bloodIntensity{0};
// Update 38.5: EXTREME's VISOR DROPS (live.txt blood_visor), default OFF; nothing below EXTREME.
std::atomic<bool> bloodVisor{false};
// Update 37: HOLO REUSE (perf37.hpp; live.txt holo_reuse, SpidyHoloReuse), ON by default.
std::atomic<bool> holoReuseOn{true};
// Update 37: what one part of the frame tells a later one (the main loop's thread only).
struct Update37Frame {
    float ball[2]{}, other[2]{};   // the web-ball button, the other (web) button
    bool taken[2]{};               // the ball claimed by a scan, the hologram or a prompt
    bool holdsGrip[2]{};           // combat holds that hand's web button from the swing
    bool climbing[2]{};            // the hand holds the wall to climb
    bool healPressed{};
};
Update37Frame u37;
combat_input::Machine combatMachine;
hand_crawl::Climber climber;
health_log::Log healthLog;
template <class D, class F> void publish(D& d, F fill) {
    ++d.sequence;
    std::atomic_thread_fence(std::memory_order_release);
    fill(d);
    std::atomic_thread_fence(std::memory_order_release);
    ++d.sequence;
}
// Update 23: where webs are drawn leaving the hand (live.txt web_start,
// SpidyWebStart; tracking.hpp webStart): 0 the drawn wrist's web shooter (the
// default), 1 Update 20's spot under the palm. Only the picture: where webs
// aim and attach is the same either way.
std::atomic<uint32_t> webStartSpot{webStartWrist};
// Update 16: the aim reticles shown (live.txt reticles, SpidyReticles); only the
// picture: the aim previews, the grabs and the swings are the same either way.
std::atomic<uint32_t> reticlesShown{1};
// Update 33: EYE AA (eye_taa.hpp), eye_taa::pack(state, reason): live, default OFF, never
// kept. On, each eye command's lens moves a sub-pixel step and the history pass runs at
// least eye_taa::historyPercent; a failed pass turns it off with its reason (one line).
std::atomic<uint32_t> eyeAaState{};
uint32_t eyeAaNow() {
    uint32_t s = eyeAaState.load();
    if ((s & 0xff) == eye_taa::on) {
        if (const uint32_t fault = native_gpu::eyeHistoryFault()) {
            native_gpu::setEyeTemporal(false);
            const uint32_t failed = eye_taa::pack(eye_taa::failed, fault < eye_taa::reasons ? fault : 0);
            if (eyeAaState.compare_exchange_strong(s, failed)) {
                char line[128];
                std::snprintf(line, sizeof line, "EYE AA: off, %s (history fault %u)\n",
                              eye_taa::reasonText(failed >> 8), fault);
                OutputDebugStringA(line);
            }
            return eyeAaState.load();
        }
    }
    return s;
}
// Update 34: ENGINE TAA's FPS guard is armed by the frame loop (it has the time).
std::atomic<bool> engineTaaArm{};
std::atomic<bool> farShadowsArm{}; // Update 35: FAR SHADOWS' own FPS guard, the same way
void pressEyeAa() {
    const uint32_t s = eyeAaNow();
    if ((s & 0xff) == eye_taa::on) {
        native_gpu::setEyeTemporal(false);
        eyeAaState = eye_taa::pack(eye_taa::off, 0);
        OutputDebugStringA("EYE AA: off\n");
    } else if ((s & 0xff) == eye_taa::off) {
        // Update 34: one anti-aliasing at a time: EYE AA on turns ENGINE TAA off.
        if (engine_taa::stateOf(SpidyEngineTaaNow()) == engine_taa::on)
            SpidyEngineTaa(nullptr);
        if (native_gpu::setEyeTemporal(true)) {
            eyeAaState = eye_taa::pack(eye_taa::on, 0);
            OutputDebugStringA("EYE AA: on\n");
        } else {
            const uint32_t fault = native_gpu::eyeHistoryFault();
            eyeAaState = eye_taa::pack(eye_taa::failed, fault < eye_taa::reasons ? fault : 0);
            OutputDebugStringA("EYE AA: refused, the history pass has failed\n");
        }
    } // failed: stays off for the session (the reason stays shown)
}
// Update 34: ENGINE TAA on/off. On is refused (the reason shown) unless both eyes have
// the TAA targets; on turns EYE AA off; the FPS guard watches the next 3 s.
void pressEngineTaa() {
    if (engine_taa::stateOf(SpidyEngineTaaNow()) == engine_taa::on) {
        SpidyEngineTaa(nullptr);
        return;
    }
    if (SpidyEngineTaa(reinterpret_cast<void*>(uintptr_t{1})) != 0)
        return;
    if ((eyeAaNow() & 0xff) == eye_taa::on) {
        native_gpu::setEyeTemporal(false);
        eyeAaState = eye_taa::pack(eye_taa::off, 0);
        OutputDebugStringA("EYE AA: off (ENGINE TAA is on)\n");
    }
    engineTaaArm = true;
}
// Update 34: plan B. ENGINE TAA went off on its own (targets gone, frame rate halved):
// EYE AA goes on in its place, if it can.
void engineTaaFallback(uint32_t reason) {
    if (reason != engine_taa::targetsGone && reason != engine_taa::slow)
        return;
    if ((eyeAaNow() & 0xff) == eye_taa::off)
        pressEyeAa();
}
// Update 24: the session FPS (wrist_hud::FpsSession), published by the XR
// thread for SpidyHudSettings(8/9): tenths of a frame a second (0 none) and
// the whole seconds tracked.
std::atomic<uint32_t> sessionFpsAvg{}, sessionFpsHigh{}, sessionFpsLow{}, sessionFpsSeconds{};
// Update 18: the webs' look (live.txt web_style, SpidyWebStyle; web_visual.hpp
// web_style) and each style's own shade, kept per style (web_color_<style>,
// SpidyWebColor). Only the picture: the swing is the same in every style.
// Update 20: MESSY by default (was 90s).
std::atomic<uint32_t> webStyleNow{1};
std::atomic<uint32_t> webShades[web_style::count]{};
// The wrist hologram (Update 14, wrist_hud.hpp): live.txt hud (on), hud_hand
// (0 the left wrist, 1 the right) and hud_scale (percent). wristHudFault: it
// failed once (threw, or drew something unfit), and only it is off until
// live.txt's hud is set again.
std::atomic<bool> wristHudOn{true}, wristHudFault{};
std::atomic<uint32_t> wristHudHand{0}, wristHudScalePercent{100};
// Update 17: the HOLOGRAM page's (live.txt hud_height cm 0-25, hud_text
// percent 75-175, hud_theme 0-5 and hud_text_color 0-3; wrist_hud.hpp).
std::atomic<uint32_t> wristHudHeightCm{9}, wristHudTextPercent{100}, wristHudTheme{0}, wristHudTextColor{0};
// Update 38.5: the scrolling subtext's control << 8 | (its overflow + 1), from the drawing side.
std::atomic<uint32_t> wristHelpOverflow{0};
// Update 15: the hologram's pages. wristMenuFault: its controls failed once
// (threw) and only they are off (the panel still shows) until live.txt's hud
// is set again. nextSupersample: the GRAPHICS page's NEXT LAUNCH eye
// supersample (0 none chosen): never applied live, read back by the launcher
// (SpidyHudSettings) into next-launch.txt.
std::atomic<bool> wristMenuFault{};
std::atomic<uint32_t> nextSupersample{0};
// Update 19: the MUSIC page's (live.txt music_volume percent 0-100,
// music_visualizer 0-4 music::visualizerName, music_track the song from 1,
// 0 none). musicTrackStart: the launch's music_track, picked (not played)
// once the songs are found; musicTrackNow the song picked or playing, for
// the launcher to keep (SpidyHudSettings 7). Only the mod's music: the
// game's own sound is never touched.
std::atomic<uint32_t> musicVolumeNow{spidy::music::defaultVolume}, musicVisualizerNow{0};
std::atomic<int32_t> musicTrackStart{-1}, musicTrackNow{-1};
// The settings (1 << wrist_hud::Control) the pages changed and the mod took,
// since the launcher last asked (SpidyHudSettings 3): only those it writes.
std::atomic<uint32_t> wristChanged{};
// Why a gameplay bridge refused to start (SpidyRayStart, SpidySwingStart and
// the movement module's SpidyMotionStart share these codes).
std::string gameplayRefusal(uint32_t code) {
    if (code == 1)
        return "the bridge's start function is missing";
    if (code == 1000)
        return "already running";
    if (code == 1001)
        return "refused its configuration or the game module (Spider-Man2.exe only)";
    if (code == 1002)
        return "this game build's bytes differ from SM2VR 0.0.27's (rays), or motion exports missing";
    if (code == 1003)
        return "this game build's bytes differ from SM2VR 0.0.27's (movement)";
    if (code >= 1100 && code < 1200)
        return "MinHook could not initialise";
    if (code >= 1200 && code < 1300)
        return "MinHook could not create a hook (another mod hooking the same function?)";
    if (code >= 1300 && code < 1400)
        return "MinHook could not enable a hook";
    return "refused";
}
void message(uint32_t status, uint32_t error, const char* text) {
    std::lock_guard lock(telemetry);
    InterlockedIncrement64(&SpidyXrData.sequence);
    SpidyXrData.status = status;
    SpidyXrData.error = error;
    strncpy_s(SpidyXrData.message, text, _TRUNCATE);
    InterlockedIncrement64(&SpidyXrData.sequence);
}
// Build 5 (0.0.72 test): lines for the launcher console that must not be lost. message() holds one line and
// the launcher prints it only when it changes (polled every 50-100 ms), so lines written close together
// vanished (build 4's model notes never reached the console). consoleLine() queues; drainConsoleLines()
// (the VR loop) shows one line every 250 ms, keeping the current status and error. Never throws.
std::mutex consoleLinesLock;
int thorStepsLogged = 0; // Version 0.0.72 build 6: THOR's console steps (game_xr's frame loop only)
std::deque<std::string> consoleLinesQueue;
void consoleLine(const std::string& text) noexcept {
    try {
        OutputDebugStringA((text + "\n").c_str());
        std::lock_guard lock(consoleLinesLock);
        if (consoleLinesQueue.size() < 64)
            consoleLinesQueue.push_back(text.substr(0, 400));
    } catch (...) {
    }
}
void drainConsoleLines() noexcept {
    try {
        static auto last = std::chrono::steady_clock::time_point{};
        const auto now = std::chrono::steady_clock::now();
        if (now - last < std::chrono::milliseconds(250))
            return;
        std::string line;
        {
            std::lock_guard lock(consoleLinesLock);
            if (consoleLinesQueue.empty())
                return;
            line = std::move(consoleLinesQueue.front());
            consoleLinesQueue.pop_front();
        }
        last = now;
        const uint32_t status = SpidyXrData.status;
        if (status == 4 || status == 5) // VR ended / failed: those lines stay as they are
            return;
        message(status, SpidyXrData.error, line.c_str());
    } catch (...) {
    }
}
bool read(uintptr_t p, void* out, size_t bytes) {
    __try {
        if (p < 0x10000)
            return false;
        std::memcpy(out, reinterpret_cast<void*>(p), bytes);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// Update 38.5 INVINCIBLE: one float written only where it still holds `expected` (a fault: nothing).
bool writeFloatIf(uintptr_t p, float expected, float value) {
    __try {
        if (p < 0x10000)
            return false;
        auto* f = reinterpret_cast<volatile float*>(p);
        if (*f != expected)
            return false;
        *f = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
void check(DWORD code, const char* operation) {
    if (code)
        throw std::runtime_error(std::string(operation) + ": " + std::to_string(code));
}
// Publishes a completed eye copy, or withdraws the previous one before its
// pixels are released.
void publishSnapshot(EyeSnapshot shot, const D3D12Renderer::Captured* pixels) {
    std::lock_guard lock(telemetry);
    auto& d = SpidyXrSnapshot;
    InterlockedIncrement64(&d.sequence);
    const auto sequence = d.sequence;
    const auto count = d.count + (pixels != nullptr);
    d = shot;
    d.sequence = sequence;
    d.count = count;
    if (pixels) {
        d.pixels = reinterpret_cast<uint64_t>(pixels->pixels);
        d.width = pixels->width;
        d.height = pixels->height;
        d.rowPitch = pixels->rowPitch;
    }
    InterlockedIncrement64(&d.sequence);
}
struct RuntimeSelection {
    std::wstring previous;
    bool present{};
    explicit RuntimeSelection(const wchar_t* requested) {
        const DWORD size = GetEnvironmentVariableW(L"XR_RUNTIME_JSON", nullptr, 0);
        present = size != 0;
        if (size) {
            previous.resize(size);
            GetEnvironmentVariableW(L"XR_RUNTIME_JSON", previous.data(), size);
            previous.resize(size - 1);
        }
        constexpr auto virtualDesktop =
            L"C:\\Program Files\\Virtual Desktop Streamer\\OpenXR\\virtualdesktop-openxr.json";
        const wchar_t* manifest = *requested ? requested : virtualDesktop;
        if (GetFileAttributesW(manifest) == INVALID_FILE_ATTRIBUTES) {
            if (*requested)
                throw std::runtime_error("The chosen OpenXR runtime is not installed");
            return; // no Virtual Desktop: the loader uses Windows' active runtime
        }
        if (!SetEnvironmentVariableW(L"XR_RUNTIME_JSON", manifest))
            throw std::runtime_error("Could not select the OpenXR runtime for this process");
    }
    ~RuntimeSelection() {
        SetEnvironmentVariableW(L"XR_RUNTIME_JSON", present ? previous.c_str() : nullptr);
    }
};
// OpenXR add-ons Spidy cannot run with (xr_layers.hpp), switched off in this
// process while the loader reads them: as it creates the instance. Each by
// the variable its own manifest names; one the player has set stays as it is.
struct LayerSwitches {
    std::vector<std::string> variables;
    // The layers switched off, for the player.
    std::string names;
    explicit LayerSwitches(std::span<const xr_layers::Layer> layers = xr_layers::unsupported) {
        for (const HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
            HKEY key{};
            if (RegOpenKeyExW(root, L"SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit", 0, KEY_READ, &key) !=
                ERROR_SUCCESS)
                continue;
            // Each value: a manifest's path, and 0 while its layer is on.
            for (DWORD index = 0; index < 256; ++index) {
                wchar_t path[1024];
                DWORD chars = static_cast<DWORD>(std::size(path)), type{}, off{}, bytes = sizeof(off);
                const auto code = RegEnumValueW(key, index, path, &chars, nullptr, &type,
                                                reinterpret_cast<BYTE*>(&off), &bytes);
                if (code == ERROR_NO_MORE_ITEMS)
                    break;
                if (code != ERROR_SUCCESS || type != REG_DWORD || off)
                    continue;
                const HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                nullptr, OPEN_EXISTING, 0, nullptr);
                if (file == INVALID_HANDLE_VALUE)
                    continue;
                std::string text(65536, '\0');
                DWORD size{};
                const bool loaded =
                    ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &size, nullptr);
                CloseHandle(file);
                if (!loaded)
                    continue;
                text.resize(size);
                const auto layer = xr_layers::switchOff(text, layers);
                if (layer.variable.empty() || GetEnvironmentVariableA(layer.variable.c_str(), nullptr, 0) ||
                    !SetEnvironmentVariableA(layer.variable.c_str(), "1"))
                    continue;
                variables.push_back(layer.variable);
                names += (names.empty() ? "" : ", ") + std::string(layer.name);
            }
            RegCloseKey(key);
        }
    }
    ~LayerSwitches() {
        for (const auto& variable : variables)
            SetEnvironmentVariableA(variable.c_str(), nullptr);
    }
};
// Where the settings only the SPIDY VR tab offers are kept
// (vr_settings::tabOnlyText): beside the launcher's launcher.ini, so every
// Spidy folder and every build finds them (this module runs from a copy
// staged per build).
std::wstring tabOnlyPath() {
    PWSTR folder{};
    std::wstring path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &folder)) && folder)
        path = std::wstring(folder) + L"\\Spidy\\spidy_vr.ini";
    CoTaskMemFree(folder);
    return path;
}
std::string readTabOnly() {
    const auto path = tabOnlyPath();
    if (path.empty())
        return {};
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return {};
    std::string text(4096, '\0');
    DWORD size{};
    if (!ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &size, nullptr))
        size = 0;
    CloseHandle(file);
    text.resize(size);
    return text;
}
// Written whole and then put in place, so a crash mid-write keeps the last.
bool writeWhole(const std::wstring& path, const std::string& text);
bool writeTabOnly(const std::string& text) {
    return writeWhole(tabOnlyPath(), text);
}
bool writeWhole(const std::wstring& path, const std::string& text) {
    if (path.empty())
        return false;
    CreateDirectoryW(path.substr(0, path.find_last_of(L'\\')).c_str(), nullptr);
    const auto written = path + L".new";
    const HANDLE file = CreateFileW(written.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    DWORD size{};
    const bool whole = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &size, nullptr) &&
                       size == text.size();
    CloseHandle(file);
    return whole &&
           MoveFileExW(written.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}
// Update 38.5 GESTURE REMAP: the player's own motions, per profile, beside spidy_vr.ini
// (%APPDATA%\\Spidy\\gestures.txt) - outside the mod's folder, so every update keeps them.
std::wstring gesturesPath() {
    const auto ini = tabOnlyPath();
    return ini.empty() ? ini : ini.substr(0, ini.find_last_of(L'\\')) + L"\\gestures.txt";
}
gesture_remap::Store loadGestures() {
    try {
        const auto path = gesturesPath();
        if (path.empty())
            return {};
        const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                        OPEN_EXISTING, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return {};
        LARGE_INTEGER size{};
        std::string text;
        if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < (4 << 20)) {
            text.resize(static_cast<size_t>(size.QuadPart));
            DWORD read{};
            if (!ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &read, nullptr))
                read = 0;
            text.resize(read);
        }
        CloseHandle(file);
        return gesture_remap::load(text);
    } catch (...) {
        return {}; // a bad file: the built-in moves, never a crash
    }
}
// Version 0.0.72 (TEST) HERO PROGRESS: reputation, suits and perks kept beside spidy_vr.ini
// (%APPDATA%\\Spidy\\hero072_progress.txt), outside the mod's folder, so an update keeps them.
std::wstring hero072ProgressPath() {
    const auto ini = tabOnlyPath();
    return ini.empty() ? ini : ini.substr(0, ini.find_last_of(L'\\')) + L"\\hero072_progress.txt";
}
std::string readHero072Progress() {
    try {
        const auto path = hero072ProgressPath();
        if (path.empty())
            return {};
        const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                        OPEN_EXISTING, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return {};
        std::string text(16384, '\0');
        DWORD size{};
        if (!ReadFile(file, text.data(), static_cast<DWORD>(text.size() - 1), &size, nullptr))
            size = 0;
        CloseHandle(file);
        text.resize(size);
        return text;
    } catch (...) {
        return {}; // a bad file: a fresh start, never a crash
    }
}
// REALISTIC FLIPS and the rows under it, as the rig takes them.
RealisticFlipSettings realisticFlips(const vr_settings::Values& v) {
    return {v.realisticFlips,         v.automaticReleaseFlip,  v.automaticStrength,
            v.automaticSpeed,         v.automaticMinimumSpeed, v.automaticResponse,
            v.automaticHandStrength,  v.automaticFalloff,      v.automaticHandThreshold,
            v.automaticAirBrake};
}
// Version 0.0.2 hotfix: the VR thread's stack reserve (see SpidyXrStart).
constexpr SIZE_T xrThreadStackReserve = 8u << 20;
DWORD WINAPI run(void*) {
    bool nativeStarted = false, gpuStarted = false, raysStarted = false, swingStarted = false,
         raysFailed = false, swingFailed = false, websStarted = false,
         bodyStarted = false;
    // Version 0.0.72 GAME BODY: the checks' state last logged, and the one a hook was last tried at.
    uint32_t gameBodySerial = ~0u, gameBodyTried = ~0u;
    std::string gameBodyRigRefusal;
    uint64_t serial = 1, nativeDeadline{};
    // lastPresentedMs: an image was on show. lastNewImageMs: the game delivered a newer one.
    uint64_t firstImageDeadline{}, lastPresentedMs{}, lastNewImageMs{};
    bool presented{}, wasGameplay{};
    // Update 32.1: eye images taken over / the eyes' pause, as last messaged.
    uint32_t eyeSwapsSeen{};
    bool eyesLostSeen{};
    // The last frame's predicted display time; held images age against it.
    int64_t displayTime{};
    Presentation shownKind{};
    uint32_t error{};
    std::string failure;
    bool runtimeEnded = false; // Update 24.1: the VR runtime closed the session
    // VR starts with the game, before there is a player; each loaded save,
    // respawn or character switch brings a new one.
    game_player::Watch players;
    bool padInstalled{};
    try {
        auto queue = reinterpret_cast<ID3D12CommandQueue*>(config.queue);
        ComPtr<ID3D12Device> device;
        if (FAILED(queue->GetDevice(IID_PPV_ARGS(&device))))
            throw std::runtime_error("Game graphics device unavailable");
        XrRuntime runtime;
        {
            RuntimeSelection selected(config.runtime);
            LayerSwitches layers;
            if (!layers.names.empty()) {
                const auto text = layers.names + " is switched off for the game: Spidy does not run with it. "
                                                 "It stays on for your other games";
                message(1, 0, text.c_str());
            }
            // Update 39.0.3: no headset yet (Virtual Desktop not connected) waits for it, up to 2 minutes,
            // instead of ending VR with the game on its flat screen.
            // 0.0.6 hotfix (Update 39.1.1, rep0060): any other OpenXR refusal at the start (Virtual Desktop's
            // "Create eye swapchain XrResult=-2") is tried again too, with a new session every 2 s (5 s after
            // 2 minutes), for as long as the launcher is there; each eye swapchain try is said in the console.
            // VR no longer ends on it with the game on its flat screen.
            const auto sayTries = [&](const char* prefix) {
                for (const auto& line : runtime.swapchainTries()) {
                    const auto text = std::string(prefix) + line;
                    OutputDebugStringA((text + "\n").c_str());
                    message(2, 0, text.c_str());
                    Sleep(150); // the launcher reads the message every 50 ms: each line reaches the console
                }
            };
            const uint64_t startedAt = GetTickCount64();
            for (unsigned startTries = 1;; ++startTries) {
                try {
                    runtime.initialize(device.Get(), queue, config.eyeSize, config.renderScale);
                    if (startTries > 1 || !runtime.swapchainNote().empty()) {
                        sayTries("");
                        const auto text = (runtime.swapchainNote().empty()
                                               ? std::string("VR started")
                                               : runtime.swapchainNote()) +
                                          " after " + std::to_string(startTries) + " session tries";
                        message(2, 0, text.c_str());
                    }
                    break;
                } catch (const XrError& e) {
                    const bool launcherGone = !config.durationMs && GetTickCount64() >= keepAliveDeadline;
                    if (stopRequested || launcherGone)
                        throw;
                    const uint64_t waited = GetTickCount64() - startedAt;
                    if (e.result == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
                        if (waited >= 120000)
                            throw;
                        message(2, 0, "Headset not connected: open Virtual Desktop in your headset first");
                        Sleep(2000);
                        continue;
                    }
                    sayTries("");
                    const auto text = std::string(waited < 120000 ? "VR is starting: the runtime refused (" :
                                                                    "VR could not start yet (") +
                                      e.what() + "); trying again, session try " + std::to_string(startTries + 1) +
                                      (waited < 120000 ? "" : ". If it goes on: restart Virtual Desktop and the game");
                    OutputDebugStringA((text + "\n").c_str());
                    message(2, 0, text.c_str());
                    Sleep(waited < 120000 ? 2000 : 5000);
                }
            }
        }
        D3D12Renderer overlay;
        overlay.initialize(device.Get(), queue);
        std::string artMissing;
        // Update 36: the calibration panel's pictures (calibration_art.hpp),
        // decoded from the DLL and uploaded once. Should that fail, the
        // calibration shows its previous panel.
        {
            calibration_art::Image atlas;
            std::vector<calibration_art::Image> levels;
            if (calibration_art::load(atlas))
                levels = calibration_art::mips(atlas, 5);
            std::vector<D3D12Renderer::ArtLevel> art;
            for (const auto& level : levels)
                art.push_back({static_cast<unsigned>(level.width), static_cast<unsigned>(level.height),
                               level.rgba.data()});
            // (Said when a calibration shows the plain panel: a message now
            // would be replaced at once by the wait for the headset.)
            if (art.empty())
                artMissing = "its pictures did not decode";
            else if (!overlay.setArt(art))
                artMissing = overlay.artFault();
            if (!artMissing.empty())
                OutputDebugStringA(("Spidy calibration art: " + artMissing).c_str());
        }
        // Version 0.0.72 REAL MODELS: every drop-in model read now (not on first use), its textures
        // decoded (WIC, on a thread of its own for COM), scaled to one texture array and uploaded once;
        // should any of that fail the models are drawn untextured (their light copies) - never a crash.
        // Build 5: build 4 made that array at 1024 px (29 layers: some 160 MB in memory, another 160 MB
        // upload buffer, another 160 MB on the GPU) on a PC with 297 MB of commit left - the upload failed
        // and every model was drawn as its flat-coloured light copy, said only in a report nobody saw.
        // Now: 512 px (256 px when commit is short; about 40 / 10 MB), the encoded images freed once
        // used, every step guarded (out of memory included) and each result on the launcher console.
        try {
            auto& models = mesh_model::library();
            const std::string root = modelsRoot();
            const bool rootThere = GetFileAttributesA(root.c_str()) != INVALID_FILE_ATTRIBUTES;
            consoleLine(std::string("MODELS: folder ") + root + (rootThere ? "" : " - NOT FOUND (MODEL MISSING: the built-in looks are drawn)"));
            uint32_t loadedCount = 0;
            std::string missingSlots;
            for (uint32_t slot = 1; slot < mesh_model::slots; ++slot) {
                mesh_model::Status status = mesh_model::invalid;
                try {
                    status = models.load(slot, root);
                } catch (...) { // out of memory or worse: this slot is MODEL MISSING, nothing else stops
                    models.meshes[slot] = {};
                    models.status[slot] = mesh_model::invalid;
                    models.error[slot] = "could not be read (out of memory?)";
                }
                char line[400];
                sprintf_s(line, "Version 0.0.72 REAL MODELS %s: %s %u triangles%s%.300s", mesh_model::slotFolders[slot],
                          mesh_model::statusNames[status < 5 ? static_cast<int>(status) : 0],
                          models.model(slot) ? models.model(slot)->triangles() : 0u,
                          status == mesh_model::loaded ? "" : " - the built-in look drawn: ", models.error[slot].c_str());
                game_prompts::note(line);
                OutputDebugStringA(line);
                if (models.model(slot))
                    ++loadedCount;
                else {
                    missingSlots += missingSlots.empty() ? "" : ", ";
                    missingSlots += mesh_model::slotFolders[slot];
                    consoleLine(line);
                }
            }
            consoleLine("MODELS: " + std::to_string(loadedCount) + " of " + std::to_string(mesh_model::slots - 1) + " loaded" +
                        (missingSlots.empty() ? "" : " (MODEL MISSING: " + missingSlots + ")"));
            // Build 9 SUITS: each loaded suit made ready to wear (its skin welded, each triangle's bone).
            {
                uint32_t rigged = 0;
                std::string unrigged;
                suitsRigged = false;
                for (uint32_t k = 0; k < mesh_model::suitSlots; ++k) {
                    suitRigs[k] = {};
                    const mesh_model::Mesh* m = models.model(mesh_model::slotSuitFirst + k);
                    bool ok = false;
                    try {
                        ok = m && suit_wear::makeRig(*m, suitRigs[k], mesh_model::suitFolders[k]);
                    } catch (...) {
                        suitRigs[k] = {};
                    }
                    rigged += ok;
                    if (m && !ok)
                        unrigged += (unrigged.empty() ? "" : ", ") + std::string(mesh_model::suitFolders[k]);
                }
                suitsRigged = true;
                consoleLine("SUITS: " + std::to_string(rigged) + " of " + std::to_string(mesh_model::suitSlots) + " ready to wear" +
                            (unrigged.empty() ? "" : " (NOT WEARABLE: " + unrigged + ")"));
            }
            MEMORYSTATUSEX memory{sizeof(memory)};
            const uint64_t commitFree = GlobalMemoryStatusEx(&memory) ? memory.ullAvailPageFile : 0;
            // Build 7: 512 px from 768 MB of free commit (29 layers: about 40 MB with mips). Builds 5-6 asked
            // for 3 GB, so the user's 1228 MB got 256 px and every model looked blurred (the test-6 log).
            // Build 9: with the suits about 110 layers (about 150 MB at 512 px, the same again while uploading).
            const uint32_t size = commitFree >= (768ull << 20) ? 512u : 256u;
            uint32_t levels = 1;
            while ((size >> levels) >= 1)
                ++levels;
            mesh_model::TextureArray textures;
            try {
                std::thread decode([&] {
                    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                    try {
                        textures = mesh_model::prepareTextures(models, &wicDecode, size, levels);
                    } catch (...) {
                        textures = {};
                        textures.notes.push_back("the textures could not be prepared (out of memory?)");
                    }
                    if (SUCCEEDED(com))
                        CoUninitialize();
                });
                decode.join();
            } catch (...) {
                textures.notes.push_back("no thread for the textures");
            }
            // The encoded images are not needed again (their layers are made): give the memory back.
            for (uint32_t slot = 1; slot < mesh_model::slots; ++slot)
                for (auto& im : models.meshes[slot].images)
                    std::vector<uint8_t>().swap(im.bytes);
            for (const auto& note : textures.notes) {
                game_prompts::note(("Version 0.0.72 REAL MODELS texture: " + note).c_str());
                OutputDebugStringA(note.c_str());
                if (note.find("layer") == std::string::npos || note.find("not") != std::string::npos)
                    consoleLine("MODELS texture: " + note);
            }
            char summary[200];
            sprintf_s(summary, "MODELS: textures %zu layers at %u px (free commit %llu MB)", textures.layers.size(), size,
                      static_cast<unsigned long long>(commitFree >> 20));
            consoleLine(summary);
            bool texturedOk = false;
            if (!textures.layers.empty()) {
                texturedOk = overlay.setModelTextures(textures.size, textures.levels, textures.layers);
                if (!texturedOk) {
                    game_prompts::note(("Version 0.0.72 REAL MODELS untextured: " + overlay.modelFault()).c_str());
                    OutputDebugStringA(overlay.modelFault().c_str());
                }
            }
            textures = {}; // the CPU copies: uploaded (or refused); freed now
            consoleLine(texturedOk && overlay.modelReady()
                            ? std::string("MODELS: TEXTURED - the real models are drawn")
                            : "MODELS: UNTEXTURED - the flat light copies are drawn: " +
                                  (overlay.modelFault().empty() ? std::string("no textures were read") : overlay.modelFault()));
        } catch (...) {
            consoleLine("MODELS: could not be prepared (out of memory?) - the built-in looks are drawn");
        }
        const auto artClock = std::chrono::steady_clock::now();
        // Update 24.1: a drawing failure (its shader, its buffers) turns Spidy's
        // drawings off for the session; the headset keeps the game's eyes.
        bool overlayFault = false, overlayShaderNoted = false, overlayArtNoted = false;
        // The session report's copies of the left eye live in the overlay's
        // readback buffer; withdraw the last one before the overlay goes.
        struct WithdrawSnapshot {
            ~WithdrawSnapshot() {
                publishSnapshot({}, nullptr);
            }
        } withdrawSnapshot;
        EyeSnapshotSchedule snapshots;
        EyeSnapshot pendingSnapshot{};
        uint64_t snapshotTicket{}, gapGeneration{};
        message(2, 0, "Waiting for the headset");
        players.start(config.base);
        world_trace::start(config.base); // Update 37: the read-only WORLD LOG (its own low thread)
        // Version 0.0.5 DAMAGE TEST (sm2_damage.hpp): its checks once (prologue, vtable); closed with a
        // code otherwise. Nothing is ever sent unless DAMAGE TEST is ON (OFF by default).
        if (const uint32_t damageCode = sm2_damage::start(config.base)) {
            char line[160];
            sprintf_s(line, "VERSION 0.0.5 DAMAGE TEST: closed, code %u (the game's damage code is not the one read)", damageCode);
            game_prompts::note(line);
        }
        // Update 38.5 BALL LOOK WRIST: the game's web ball from the pulled hand's wrist
        // (CANDIDATE muzzle; checks its bytes and vtable slots, else stays off: the game's ball).
        // Why it stayed off is in its own record (WMUZ start_error), not the VR status.
        wrist_muzzle::start(config.base);
        // Version 0.0.5 SOUND EFFECTS (sfx.hpp): XAudio2 on its own thread; the VR thread only asks.
        // It cannot fail the session: silent, with its state in the log (VERSION 0.0.5 SFX).
        sfx::setGain(combat38::sfxGainOf(c38(combat38::sfxVolume)));
        sfx::start();
        npc_fade::start(config.base);    // Update 37.2: NPC NEAR FADE (its own low thread)
        game_player::Player player{};
        uint64_t playerChanges{}, playersFound{};
        // Update 38.5 HERO SWITCH PROBE (hero_switch.hpp): a real Peter / Miles switch, re-attached.
        hero_switch::Watch heroSwitch;
        uint32_t heroSwitchSeen{}, heroSwitchPending{};
        // Update 38.5 Y IN THE AIR (air_watch.hpp): Y presses sent, in the swing's flight, GRAB WEB
        // yanks on the WEB YANK input, in the air; the swing module's AirYield counts last seen.
        uint64_t ySent{}, yInAir{}, grabYankInputs{}, yanksInAir{}, yieldsNoted{}, outlivedNoted{};
        bool ySending{};
        spidy::air_watch::YieldData airYieldState{};
        uint64_t bodySolvedWas{};
        // Menus are played with a virtual Xbox controller (game_pad).
        padInstalled = !game_pad::install();
        uint64_t padRetryMs = GetTickCount64() + 1000;
        // The VR settings are a tab of the game's own Settings (game_menu).
        const uint32_t menuCode = game_menu::install(config.base);
        if (menuCode) {
            const auto text = "No SPIDY VR tab in the game's Settings (" + std::to_string(menuCode) +
                              "); the launcher's settings apply";
            message(3, 0, text.c_str());
        }
        // The game's request for a Steam ticket, on its way back to the main
        // menu, closed every VR session that quit to it (game_steam). The
        // guard goes in once the game holds its Steam interface.
        bool steamGuarded{}, steamWarned{};
        uint64_t steamRetryMs{};
        // Slow motion slows the game's own time (game_time).
        const uint32_t timeCode = game_time::install(config.base);
        if (timeCode) {
            const auto text =
                "No slow motion: the game's time could not be hooked (" + std::to_string(timeCode) + ")";
            message(3, 0, text.c_str());
        }
        {
            std::lock_guard lock(telemetry);
            InterlockedIncrement64(&SpidySlowMotionData.sequence);
            SpidySlowMotionData.status = timeCode;
            InterlockedIncrement64(&SpidySlowMotionData.sequence);
        }
        // The last frame showed the game screen; A held from there (Resume,
        // Continue) is not a jump once gameplay is back.
        bool wasScreen{}, jumpFromScreen{};
        // A pressed in the air is not the game's web zip.
        AirJumpFilter airJump;
        // Where B, X and Y went down: on the game screen they are the game's
        // own, in VR B dodges, Y interacts and X opens the game menu
        // (game_pad::PressOrigins).
        game_pad::PressOrigins origins;
        // Y held walks the player up to what they look at: the game takes
        // its Y only within about a metre of the thing.
        game_pad::InteractStep interactStep;
        uint32_t interactStepsBefore{};
        // The game's own camera, turned to where the player looks: the game
        // takes its Y for what that camera has in view.
        game_pad::CameraLook cameraLook;
        // The game's LB + RB (its web pull): both web buttons on a thing no
        // web holds on, then a yank (WebPull); or the right thumbstick's
        // click. aimed: what each hand's web button would do, markers or not.
        WebPull webPull;
        game_pad::PullPress pullPress;
        game_swing::AimData aimed{};
        uint64_t pullsBefore{};
        // The right thumbstick pushed down is the game's D-pad down (heal),
        // pushed up its LT + RT (the zip to a point, a vent).
        game_pad::StickPush stickPush;
        game_pad::ZipPress zipPress;
        bool interacting{}, dodging{}, healing{}, zipping{};
        // The player asked the game for a move of its own while the swing carried him
        // in the air (game_swing::AirYield), for the next swing command.
        bool askingGame{};
        uint32_t interacts{}, dodges{}, heals{}, zips{};
        // The whole pad, latched by clicking both thumbsticks, for the
        // controls VR play has no room for; and the buttons the game names in
        // a prompt, sent by the VR controls of those names (game_pad.hpp,
        // game_prompts.hpp): what they send this frame, and whether the swing
        // was last told that fists are the game's X meanwhile.
        game_pad::PadLayer padLayer;
        // Update 38.5 EXTRAS (extras385.hpp): the holo's presses, the view switch, FLY's state back
        // from the swing module, SYMBIOTE LOOK's gesture and goo, INVINCIBLE's health guard.
        bool padModePressed{}, monitorPressed{}, promptHinted{};
        double extrasClock{};
        extras385::view::Switch viewSwitch;
        extras385::view::Confirm monitorConfirm;
        extras385::fly::Data flyNow{};
        // Update 38.5 FLY's poses (a hand up, both hands in front, a T-pose; arms down hovers), R3's
        // drop and L3+R3's ultimate, and FLY ROLL (a mod-side view roll, GameTrackingRig::flyRoll).
        extras385::fly::Poses flyPoses;
        // Build 7 GREEN LANTERN: RingFlight (hero072.hpp) was never called in game - one hand up took him off
        // like SUPERMAN. Now on the ground only both arms straight up (held takeoffHold) take off; in the air
        // the poses steer as for SUPERMAN.
        hero072::RingFlight glRingFlight;
        uint32_t glPose{};
        float glLift{};
        extras385::fly::StickChord flyChord;
        extras385::fly::Roll flyRollState;
        bool flyDropHeld{};
        float flyDropSeconds{};
        // Version 0.0.5 SUPERSONIC (fly_boost.hpp): the dash, its boom's time (combatClock) and the
        // flight's way then (tracking space) for the shockwave.
        fly_boost::Boost flyBoost;
        double flyBoomAt = -100;
        uint32_t flyStageShown{}, flyPoseShown{};
        extras385::symbiote::ChestPull chestPull;
        // Update 38.5 GESTURE MOVES (gestures385.hpp): the recogniser, its presses, this frame's result.
        gestures385::Recognizer gestureRec;
        gestures385::Settings gestureSet;
        gestures385::ComboSender comboSender;
        gestures385::Out gestureOut{};
        bool gestureInFlight{}, gesturePlayNow{};
        // Update 38.5 GESTURE REMAP (gesture_session.hpp): the player's own motions, recorded on
        // the hologram's EXTRAS page; the view the hologram draws; a fault turns it off.
        gesture_session::Session gestureRemap{loadGestures()};
        gesture_session::View remapView{};
        bool remapFault{};
        // Version 0.0.2 TRAINING's guide (training_guide.hpp): the move TRAIN MOVE picked and its
        // glowing path in the air (tracking space; the eyes draw trainDraw); a fault turns it off.
        training_guide::Guide trainGuide;
        training_guide::Draw trainDraw{};
        int trainMovePicked = 0;
        bool trainMine{}, trainReselect = true, trainGuideFault{};
        char trainLine[2][72]{};
        uint64_t trainSuccesses{}, trainFails{}, trainPicks{};
        extras385::symbiote::Spread symbioteSpread;
        uint32_t autoEntriesBefore{};
        game_pad::NamedPress namedPress;
        game_pad::State namedPad{};
        bool namedFists{}, fistsBlows{};
        uint32_t namedPressesBefore{};
        // The gadget wheel (X held; gadget_wheel.hpp): the player's gadgets
        // as the shooter last listed them, whether the free hand's trigger
        // fires the equipped one (off: the web shooter's ball, as before the
        // wheel), and a tap of X let go of, the game menu, held this long.
        GadgetWheel gadgetWheel;
        game_shooter::Data listed{};
        bool gadgetFire{};
        // The gadgets in the wheel's places, the web shooter's at the top, and
        // the place equipped (the web shooter's while the free hand's trigger
        // fires web balls): the wheel's, and the phone's GEAR in its order.
        const auto wheelPlaces = [&](GadgetWheelView& view) {
            view.places[0] = Gadget::webShooter;
            view.equipped = 0;
            for (uint32_t g = 0; g < std::min(listed.gadgetCount, 8u); ++g) {
                const auto& o = listed.gadgets[g];
                if (!o.kind || o.place >= gadgetPlaces)
                    continue;
                view.places[o.place] = static_cast<Gadget>(o.kind);
                view.ammo[o.place] = o.ammo;
                view.most[o.place] = o.most;
                if (gadgetFire && o.weapon == listed.equipped)
                    view.equipped = o.place;
            }
        };
        // The phone (phone.hpp): open from a back-pocket draw until B, live
        // while VR shows play; its music plays on with it away, on a thread
        // of its own (phone_music.hpp). phoneShown: what its screen shows,
        // the newest, which every image draws. phoneBlows: the swing was last
        // told the phone has the hands (Settings::noBlows).
        Phone phone;
        PocketDraw pocketDraw;
        PhoneMusic phoneMusic;
        // Update 19: the hologram's MUSIC page player (music_audio.hpp: its
        // own threads; every call returns at once, nothing it does can
        // throw here or stop the session), its newest status (read only
        // when not being written: try_lock) and what the page shows.
        music::Player musicPlayer;
        music::Player::Status musicStatus;
        wrist_hud::MusicView wristMusic;
        int musicTrackKept = -1;
        PhoneView phoneShown;
        bool phoneBlows{};
        float menuTap{};
        // The game played on its screen (VIEW: GAME SCREEN): Back and the
        // D-pad, which the VR controllers lack, besides the Xbox controller's.
        game_pad::ScreenControls screenControls;
        // Both thumbsticks held in: the game's Start, where the runtime keeps
        // the menu button (SteamVR's dashboard on a Quest's left controller).
        game_pad::PausePress pausePress;
        uint32_t padEntriesBefore{}, wallInteracts{}, pausesBefore{};
        // The headset has given a tracked head pose at least once.
        bool headSeen{};
        // The swing settings exactly as they were last sent, so the interact's
        // hand-over can put the walls back the way the player set them, and
        // never turn them on for a player who asked for them off.
        game_swing::Settings sentSwingSettings;
        bool swingSettingsSent{}, wallsHandedBack{}, toldHandoverRefused{};
        game_pad::WallHandover wallHandover;
        // What the game's own crawl does with the stick Spidy sends it, which
        // is the last unmeasured step between a wall and a vent that behaves
        // like the ground.
        CrawlStick crawlStick;
        uint32_t crawlFrames{}, crawlStood{};
        // The game's own crawl at rest and its wall run (XrData version 33).
        uint32_t gameRunFrames{}, holdGraceFrames{};
        // Version 34: frames of play the rig could not place and why; the
        // game's crawl on a floor, how long it has gone on, and the head held
        // at its height there; the body kneeling under a held head.
        uint32_t trackingGapFrames{}, trackingGapReasons{}, floorCrawlFrames{}, crawlCapFrames{};
        uint32_t kneelFrames{};
        float floorCrawlSeconds{};
        game_warp::Data warp{};
        // The game's interactions on Spidy's wall (version 35): what the game
        // gathered near the player, what each hand points at, and the
        // interact's use of it.
        game_offers::Data offers{};
        std::array<std::optional<offer_pointing::Pointed>, 2> pointing{};
        std::array<uint32_t, 2> pointedBefore{};
        offer_pointing::Use offerUse;
        uint32_t offerPointFrames{}, offerReachFrames{}, offersSeen{};
        uint32_t offerAsksBefore{}, offerFailuresBefore{}, offerAimed{};
        // The surface over the player's feet, as the last finished ray batch
        // found it: the head stops short of it, so it never goes through the
        // roof of a vent. Zero where nothing is over him.
        float headroomSeen{}, headroomLeast{};
        // Update 38 CRAWL CAP: a fresh roof reading came (a new ray batch), and the cap's rule.
        bool roofFresh{};
        uint64_t roofSerial{};
        combat38::CrawlCap crawlGuard;
        // Update 38 (SpidyVR fold-in): the enemies this frame (WORLD LOG's list: feet less the
        // anchor, and their ids), and the head's sight ray to each - one every 3rd frame, in
        // the roof ray's slot (the batch is full). What it found is kept by id; none yet is
        // unknown, which counts as in sight (combat_spidy::Sight).
        struct SightAsk {
            uint64_t serial{}, id{};
        };
        struct SightSeen {
            uint64_t id{};
            uint8_t sight{};
        };
        std::array<Vec3, world_trace::enemySlots> enemiesHere{};
        std::array<uint64_t, world_trace::enemySlots> enemiesHereIds{};
        uint32_t enemiesHereCount{}, sightFrame{}, sightNext{}, sightAskNext{}, sightSeenNext{};
        std::array<SightAsk, 4> sightAsks{};
        std::array<SightSeen, world_trace::enemySlots> sightSeen{};
        uint64_t sightRays{}, sightBlocked{};
        const auto sightOf = [&sightSeen](uint64_t id) -> uint8_t {
            for (const auto& seen : sightSeen)
                if (id && seen.id == id)
                    return seen.sight;
            return combat_spidy::sightUnknown;
        };
        // LUNGE SPIDYVR MOVE: the bridge's lunges as last sampled; a note once when the ray
        // module has none (the lunge stays GAME BUTTON's).
        combat_spidy::LungeData lungeState{};
        uint64_t lungeLandedWas{}, lungeOpenedBase{}, lungeMissedBase{};
        bool lungeBaseSet{}, lungeFallbackNoted{};
        // Update 38: the fight's input last frame (the cap holds off 1.5 s after it), the web
        // ball globs (tracking space), the hero's share for a death or a respawn, the enemy
        // near episodes, and the newest wrist for RING ANCHOR HAND.
        bool combatInputLast{}, enemyNearWas{}, latestWristTracked{};
        combat38::Glob globs[2];
        uint64_t globsLaunched{};
        // Version 0.0.2 BALL LOOK OURS (our_ball.hpp): our balls in flight (tracking space), the
        // enemies' speeds for the lead, the splashes hits leave; a fault turns OURS's balls off.
        our_ball::Pool oursBalls;
        our_ball::Motion oursMotion;
        our_ball::Splash oursSplash[our_ball::Pool::balls];
        int oursSplashNext{};
        bool oursFault{};
        web_cocoon::Set cocoons; // Version 0.0.3 WEB BALL HIT: SpidyVR's webbing count and the webbed-up look
        uint64_t oursGameHits{}, oursNotedFired{};
        double oursNotedAt = -10;
        uint64_t everyHitNoted{}; // Version 0.0.4 EVERY HIT's note (every 5 s while it sends)
        // Version 0.0.4 IRON MAN ABILITIES (REPULSOR) (repulsor.hpp): the trigger's chord and the beam's
        // cooldown, the damage ledger (CANDIDATE: counted, never sent), the beam's look.
        repulsor::Trigger repTrigger;
        repulsor::Ledger repLedger;
        // Version 0.0.5 DAMAGE PROBE (damage_probe.hpp, read-only; heap: the stack-frame gate).
        auto damageTrackerHeap = std::make_unique<damage_probe::Tracker>();
        damage_probe::Tracker& damageTracker = *damageTrackerHeap;
        float damageProbeClock{};
        uint32_t damageEventsNoted{}, damageHealthNoted{}, damageWriteNoted{};
        uint32_t sfxStateNoted = 0xffffffffu; // Version 0.0.5 SOUND EFFECTS
        bool repWas{};
        // Version 0.0.6 DAREDEVIL ABILITIES (hero_modes.hpp Baton): a baton per hand, its sounds' count,
        // its clock; the blows sent (batons, DAREDEVIL PUNCH's extra, HULK's punches).
        hero_modes::Baton batons[2];
        hero_modes::BatonSwing batonSwings[2]; // Version 0.0.62 BATON SWING: the in-hand batons' hits
        uint64_t batonSwingHits{};
        uint32_t batonThrowsSeen[2]{};
        double batonAt = -1;
        uint64_t batonBlowsSent{}, ddPunchBlows{}, hulkPunchBlows{}, heroPunchMisses{};
        // Version 0.0.62 (the SpidyVR comparison): PUNCH HITS' instant blows sent / with no enemy near,
        // and the frames ENEMY TRACKING placed the enemies by world position (LIVE) or by offset.
        uint64_t instantBlows{}, instantMisses{}, foesLiveFrames{}, foesOldFrames{};
        hit_marks::Marks hitMarks; // Version 0.0.62 HIT INDICATOR (tracking space; drawn only while ON)
        // Version 0.0.62 BOLT WALLS (bolt_walls.hpp): each flying shot's wall and the hands' aim walls,
        // WORLD space (the ray bridge's own; turned into this frame's tracking space where used); the
        // chest beam's cast and its blows held until the answer comes (at most beamWaitSeconds).
        bolt_walls::Wall shotWallsWorld[our_ball::Pool::balls]{}, handWallsWorld[2]{};
        uint32_t shotWallSerial[our_ball::Pool::balls]{}, shotAskedSerial[our_ball::Pool::balls]{};
        uint64_t shotAskedFrame[our_ball::Pool::balls]{}, wallFrame{};
        uint32_t shotCastNext{};
        uint64_t wallCasts{}, wallAnswers{}, wallBlocked{}, beamClipped{}, beamUnclipped{};
        struct BeamWait {
            bool waiting{}, asked{};
            uint32_t serial{};
            float firedAt{};
            Vec3 fromWorld{}, dirWorld{};
            bolt_walls::Wall wall{};
            int count{};
            uint64_t ids[our_ball::maxEnemies]{};
            float along[our_ball::maxEnemies]{}; // metres down the beam to each one hit
        } beamWait;
        static constexpr float beamWaitSeconds = .15f;
        uint64_t v0062Noted{};
        float v0062NotedAt = -100;
        uint32_t heroWas{};
        Vec3 batonHand[2]{}, batonAimNow[2]{}; // the hands as the batons last saw them (tracking)
        // Version 0.0.7 BATON GRIP: the held baton from the controller's grip pose (baton_grip.hpp;
        // batonShaftOk false: no usable grip pose, 0.0.62's shaft along the aim instead).
        Vec3 batonButt[2]{}, batonTipHeld[2]{};
        bool batonShaftOk[2]{};
        bool batonTracked[2]{};
        // Version 0.0.7 HERO PROFILES (hero007_play.hpp): BATMAN, WOLVERINE, SUPERMAN, PUNISHER and
        // DAREDEVIL's BATON HOLSTERS; the last frame's input (tracking space) for the draw.
        hero007::Play hero007Play;
        hero007::PlayInput hero007In;
        // Version 0.0.72 (TEST) (hero072_play.hpp): THOR, GREEN LANTERN, the list's rules, reputation,
        // suits, perks, ultimates and BATMAN's PERCH TAKEDOWN, for every hero. The health lost since the
        // last step (the health read is after the combat step), NATURAL INVINCIBLE's ask, the progress
        // file's load and last save, the last frame's look (tracking space) for the draw.
        hero072::Play hero072Play;
        hero072::Look hero072Look{};
        double hero072At = -1, hero072SavedAt = -1;
        float hero072Lost{}, hero072ShareWas = std::numeric_limits<float>::quiet_NaN();
        bool hero072LostKnown{}, hero072KeepTopped{}, hero072Loaded{};
        uint64_t hero072Sent{}, hero072Unsent{}, hero072Refills{}, hero072Perches{};
        // Version 0.0.7 BATMAN (cape_glide.hpp): the grapple presses for the next swing command, and
        // the bridge's glide / grapple state (counts seen: a new grapple, miss or glide buzzes once).
        bool batmanGrapple[2]{};
        // Version 0.0.72 GRAPPLE (grapple072.hpp): DAREDEVIL's cast and the PUNISHER's hook (hero072's
        // presses, for the next swing command), the hook launcher drawn, and the hands for the line.
        bool hero072Grapple[2]{}, hero072HookOut[2]{}, hero072HandTracked[2]{};
        // Version 0.0.72 GRAPPLE LINE: each hand's line drawn last frame, and when its pull began (the fly-out).
        bool grappleLineWas[2]{};
        std::chrono::steady_clock::time_point grappleLineAt[2]{};
        Vec3 hero072HandAt[2]{}, hero072HandAim[2]{};
        uint32_t hero072GrappleNotes{};
        bool vanXWas{}; // Version 0.0.72 PUNISHER VAN: the X tap's edge
        // Version 0.0.72 PUNISHER VAN SUMMON / ENGINE PREVIEW (van072.hpp): the X tap asks for a spot (vanWanted),
        // the ray batch probes it one world ray a frame (vanProbe, tags vanTag0+), the van steps in world space.
        static constexpr uint32_t vanTag0 = 1300, vanTags = 64;
        van072::Van van;
        van072::Probe vanProbe;
        bool vanWanted{}, vanAsking{}, vanSpotReady{}, vanDriving{};
        van072::Spot vanSpot{};
        uint32_t vanAskTag{}, vanAsks{}, vanAskFrames{};
        uint64_t vanAnswers{};
        float vanTrigger[2]{};
        double vanAt = -1;
        cape_glide::Data capeNow{};
        bool capeCounted{};
        uint32_t capeGrapplesSeen{}, capeMissesSeen{}, capeGlidesSeen{}, capeArrivalsSeen{};
        uint64_t hero007Sent{}, hero007Unsent{}, hero007Heals{}, superPunchBlows{};
        uint32_t hero007Claimed{};
        double hero007At = -1, hero007HoldAt = -1, healAt = -1;
        // Version 0.0.6: SUPERSONIC's reached speed (rep0050: 80 asked, 64 reached), the dash's top and all's.
        float sonicPeak{}, sonicLast{}, sonicBest{};
        bool sonicWas{};
        double repBeamAt = -100, repNotedAt = -10;
        double repReadyAt = -100; // Version 0.0.5 BEAM RING: the ready flash's start
        Vec3 repBeamFrom{}, repBeamTo{};
        uint64_t repNotedBlasts{}, repBeamHitsTotal{}, repBlowsSent{};
        double everyHitNotedAt = -10;
        uint32_t lastReject{};
        float heroShareWas = std::numeric_limits<float>::quiet_NaN();
        uint64_t ringHiddenFrames{}; // Update 38.1 RING LAYER NEWEST: frames the ring/holo hid as stale
        float ringErrorNow{}, ringErrorPeak{}, assistLast = 180.f, combatClock{}, rejectNoted = -10;
        // Update 38.2: GRAB WEB (each hand's grab, its enemy's id, the swing button's edge and the
        // last frame's enemies for the press), RING WRIST/HOLD, MIRROR SUIT and BRUTAL's coat.
        combat38::GrabWeb grabWebs[2];
        uint64_t grabIds[2]{};
        bool grabButtonWas[2]{};
        float grabSqueeze[2]{};
        Vec3 grabHandWas[2]{};
        bool grabHandSet[2]{};
        std::array<Vec3, combat38::maxEnemies> grabEnemies{};
        std::array<uint64_t, combat38::maxEnemies> grabEnemyIds{};
        uint32_t grabEnemyCount{};
        // Update 38.3: GRAB WEB ASSIST (ground only) and its count; GADGET HINT's labels.
        bool grabAirborne{};
        uint64_t grabAssisted{}, gadgetHintsShown{}, sonicHintsShown{}, sonicMissedSeen{};
        float sonicHintAt = -10;
        combat38::GadgetHint gadgetHints[2];
        Vec3 grabFrom[2]{}, grabTo[2]{}; // tracking space, for the rope
        // Version 0.0.3 ROPE GRAB (rope_grab.hpp): each hand's tether (it shares GRAB WEB's rope and
        // busy hand), its hand's last place and velocity (tracking space, relative to the head).
        rope_grab::Tether ropes[2];
        Vec3 ropeHandWas[2]{}, ropeHandVelocity[2]{};
        bool ropeHandSet[2]{};
        uint64_t ropeYanksSent{}, ropeClaps{}, ropeNoTarget{}, ropeBothOnOne{};
        uint64_t ropeWatched{};
        // Version 0.0.3 WALL STICK (wall_stick.hpp): the impact detector on the WORLD LOG's world list.
        wall_stick::Detector wallStick;
        float wallClock{}, wallAgeWas = -1;
        uint64_t wallArms{};
        std::array<Vec3, world_trace::enemySlots> wallFeet{};
        std::array<uint64_t, world_trace::enemySlots> wallIds{};
        // Version 0.0.3 MIRROR: the suit id watch (hero + 0x8c, CANDIDATE; read-only).
        mirror_view::SuitWatch suitWatch;
        float suitWatchClock{};
        uint64_t suitForgot{}, suitHeroWas{};
        Vec3 perfHeroWas{};
        bool perfHeroSet{};
        combat38::RingHold ringHold;
        float ringWristNow{}, ringWristPeak{};
        uint64_t mirrorSuitFrames{}, bloodCoatsMade{};
        uint32_t mirrorJointsLast{};
        bool mirrorInView{};
        // Update 38.3 SUIT COLOURS: the suit's colours learned from the left eye's own copies
        // (vr_mirror::SuitColours), the hero's limbs' pixels in the copy in flight, and the
        // mirror near the gaze (within 70 degrees: its own figure could be what is sampled).
        vr_mirror::SuitColours suitColours;
        uint64_t lastAttachedHero{}; // Update 38.5: the hero last attached to (a switch forgets the suit)
        vr_mirror::SuitPalette suitPalette;
        float suitPx[vr_mirror::suitSamplePoints][2]{};
        uint32_t suitPxCount{};
        bool mirrorInSight{};
        combat38::BloodCoat bloodCoats[2];
        Pose latestToWorld{}, latestWrist{};
        // Update 38.1 RING LAYER NEWEST: the newest grips (world) and that frame's display time.
        Pose latestGrips[2]{};
        bool latestGripsTracked[2]{};
        int64_t latestShown{};
        uint32_t headDroppedFrames{};
        bool pulling_{};
        bool padLayerWas{};
        // The prompt calls are copied out only when one more has come: the
        // lines are 3 kB and nothing changes between calls.
        uint64_t promptCallsBefore = ~0ull;
        uint32_t promptRefresh{}, vrButtonsPlayed{};
        // A held on the ground charges a leap of the swing's own (the game's
        // Charge Jump is a skill a save may lack), and a trigger pulled with
        // its hand at the temple is the game's R3, its scan. leapOut: the
        // leap let go of, for the swing's next commands.
        game_pad::ChargeJump chargeJump;
        game_pad::TemplePress templePress;
        // The wrist hologram's toggle: the other wrist to it and that hand's trigger.
        wrist_hud::Toggle wristToggle;
        // Update 20: F8 on the PC's keyboard toggles it too (held: once).
        bool hudKeyHeld{};
        uint32_t hudKeyPresses{}, hudKeyToggles{}, hudKeyRefused{}, hudBlocked{}, hudNotPlaying{};
        // Update 15: its pages, worked by the other hand, and the values they
        // set on their way to the mod (applied here, at the frame's start).
        wrist_hud::Menu wristMenu;
        wrist_hud::Applier wristApply;
        bool wristApplyStarted{};
        wrist_hud::Settings wristShown;
        uint32_t wristOff{}; // wristApply.disabled and a failed sharpening pass
        // Update 33: SHADOW DISTANCE while held (sent once, when let go).
        float shadowDistancePending = 0;
        bool shadowDistanceHeld = false;
        // Update 16: page 2's FPS readout, from the headset frames below (this
        // thread only: no lock).
        wrist_hud::FpsMeter wristFps;
        wrist_hud::FpsSession wristSession; // Update 24: HIGH / LOW / AVG, RESET
        // Version 0.0.3 AUTO PERFORMANCE (render thread only, but the report under perfReportLock).
        auto_perf::Controller autoPerf;
        auto_perf::PaceTarget perfPace; // 0.0.6: AUTO at half the headset rate when the full rate is out of reach
        auto_perf::Knobs perfUser{}, perfApplied{}, perfWant{};
        bool perfWas{}, perfRestoring{}, perfNewPair{}, perfStatusShown{};
        double perfPairSeconds{}, perfPairAt{}, perfRetryAt{}, perfReportAt{};
        uint32_t perfRefused{}, perfUserChanged{}, farAppliedPercent{~0u}, perfZoneNow{};
        uint64_t perfRestores{};
        char perfStatusText[64]{};
        uint64_t wristFpsSerial{}, wristFpsGeneration{};
        wrist_hud::Pointer wristPointer;
        std::string wristNote;
        bool mirrorTooLate{}; // Version 0.0.4 MIRROR VIEW TEST asked after the eyes were made: restart
        mirror_view::Stats mirrorStats{};
        uint32_t mirrorOffWhy{};       // MIRROR TEST: why it turned itself OFF (shown on the wrist; 0 none)
        double mirrorOffNoteUntil{};   // MIRROR TEST: the wrist's red line until then (wristClock)
        float mirrorLogClock{};        // MIRROR TEST: a timeline line every 10 s while it runs
        // Update 17: the hologram's clock (frame.seconds summed): the applier's
        // "now" (frame.seconds is the frame's length, not a time).
        double wristClock{};
        // The overlay's image interval (Update 15: a short shot flies for at
        // least a few of them).
        int64_t webShownBefore{};
        float webFrameSeconds = 1 / 90.f;
        uint32_t chargesBefore{}, leapsBefore{}, scansBefore{};
        float leapOut{};
        bool aWas72{};
        bool jumpBusy72{}; // build 5: see chargeJump (DAREDEVIL's cast waits)
        // The game's camera follows the head's pitch too: it offers a thing
        // at the player's feet or overhead only with that camera on it.
        game_pad::CameraPitch cameraPitch;
        // Thugs the web brought to the player's hand so far (an air yank's
        // arrival among them): in the air, time slows as one arrives.
        uint64_t catchesBefore{};
        // A dodge the game itself slows time for is its perfect dodge: for a
        // moment after B went down with the game's time at full speed. The
        // side its whoosh starts on (body_haptics' azimuth: + the right).
        float dodgeWatch{}, dodgeFrom{};
        // The VR settings: the launch options, then the SPIDY VR tab in the
        // game's Settings during play.
        vr_settings::Values values;
        values.aimMarkers = !(config.options & 128);
        values.webGrab = !(config.options & 8);
        values.body = !(config.options & 32);
        values.punch = !(config.options & 64);
        values.airWebs = !(config.options & 256);
        values.webShooter = !(config.options & 512);
        values.flips = config.options & 2048;
        values.triggerWebs = config.options & 4096;
        values.standOnWalls = !(config.options & 8192);
        values.arcadeWebs = config.options & 0x200000;
        values.swingSpeed = config.swingSpeed;
        values.snapTurn = static_cast<int>(config.snapTurn);
        values.smoothTurn = static_cast<int>(config.smoothTurn);
        values.haptics = static_cast<int>(config.haptics);
        values.screenSize = static_cast<int>(config.screenSize);
        values.weight = static_cast<int>(config.weight);
        values.hud = static_cast<int>(config.hud);
        values.flipSpeed = static_cast<int>(config.flipSpeed);
        values.screenDepth = static_cast<int>(config.screenDepth);
        values.bodyHaptics = static_cast<int>(config.bodyHaptics);
        // And those only the tab offers, as it left them (tabOnlyPath); all sanitized.
        values = vr_settings::withTabOnly(values, readTabOnly());
        std::string tabOnlySaved = vr_settings::tabOnlyText(values);
        game_menu::publish(values);
        {
            // BODY HAPTICS as the session starts: written before the first frame,
            // so a report never reads the zero of a struct not yet written.
            std::lock_guard lock(telemetry);
            InterlockedIncrement64(&SpidyHapticsData.sequence);
            SpidyHapticsData.level = static_cast<uint32_t>(values.bodyHaptics);
            InterlockedIncrement64(&SpidyHapticsData.sequence);
        }
        uint32_t settingChanges{};
        // aims are each hand's latest preview.
        uint64_t markersDrawn{};
        game_swing::AimData aims{};
        // Each hand's aim marker, steadied from eye image to eye image.
        std::array<AimMarkerMotion, 2> aimMotion;
        GameTrackingRig rig;
        GameMotionFrame motion;
        // Version 0.0.2 hotfix: the eye history is 256 frames (about 725 KB since 0.0.2 added
        // BALL LOOK OURS and the TRAINING guide to each frame), so it lives on the heap. On this
        // thread's stack it made run()'s frame about 915 KB of the thread's 1 MB, and the next
        // large callee overflowed it (the player's crash dump: 0xc00000fd in __chkstk).
        // (Out of memory: std::bad_alloc ends VR through this try's catch, which stops everything.)
        const auto historyOwner = std::make_unique<NativeEyeHistory>();
        NativeEyeHistory& history = *historyOwner;
        // Where the HUD panel faces in the room: it follows the head lazily
        // (native_hud::Follow), and starts in front of it again with play,
        // the immersive view and a recentred room.
        native_hud::Follow hudFollow;
        GameScreen gameScreen;
        // Whether the game is playing a scene on the screen (its time runs),
        // and whether that scene's stereo eyes have the HUD's panel.
        SceneClock sceneClock;
        ScenePanel scenePanel;
        bool flatScreen{};
        Pose screenPose{};
        const auto dimensions = runtime.eyeDimensions();
        // Aim markers are drawn so many pixels wide: above the headset's own
        // resolution they keep the size they have at it.
        const auto recommended = runtime.recommendedDimensions();
        const float markerPixels =
            recommended[0] ? std::max(1.f, static_cast<float>(dimensions[0]) / static_cast<float>(recommended[0])) : 1.f;
        const auto moduleDuration = config.durationMs ? config.durationMs + 3000 : 0;
        {
            std::lock_guard lock(telemetry);
            InterlockedIncrement64(&SpidyXrData.sequence);
            SpidyXrData.eyeWidth = dimensions[0];
            SpidyXrData.eyeHeight = dimensions[1];
            InterlockedIncrement64(&SpidyXrData.sequence);
        }
        game_swing::Data swingState;
        // Version 0.0.62 REAL ANCHORS: the swing's misses seen, the hands' shoot input last step, misses buzzed.
        uint32_t anchorMissesSeen{};
        bool anchorShootWas[2]{};
        uint64_t anchorMissBuzzes{};
        game_swing::Bends swingBends;
        // SM2 port, fail safe at run time: a swing or ray failure mid-session
        // (a refused command, a bad hit record, the bridge switching itself
        // off) turns that feature off with a console line and VR goes on.
        // check() here ended the October 22 session in flat screen at the
        // first web shot (swing 2104, then "Tracked swing input: 2002").
        const auto swingOff = [&](uint32_t code, const char* where) {
            if (swingStarted && stopSwing)
                stopSwing(nullptr);
            swingStarted = false;
            swingFailed = true;
            swingState = {};
            swingBends = {};
            const auto text = std::string("Native swinging is off: ") + where + " (" + std::to_string(code) +
                              "); VR continues";
            message(3, code, text.c_str());
        };
        const auto raysOff = [&](uint32_t code, const char* where) {
            if (swingStarted)
                swingOff(code, "the controller world rays it casts through stopped");
            if (raysStarted && stopRays)
                stopRays(nullptr);
            raysStarted = false;
            raysFailed = true;
            const auto text = std::string("Controller world rays and swinging are off: ") + where + " (" +
                              std::to_string(code) + "); VR continues";
            message(3, code, text.c_str());
        };
        game_swing::ClimbData climbState;
        game_swing::ClimbPulses climbPulses;
        // What each hand's web holds, when it caught a thug instead of a wall.
        game_grab::Data grabState;
        uint32_t grabPhases[2]{};
        // A grab's event pulse plays this long before the web's tension
        // takes the hand's haptics again (predicted display time, ns).
        int64_t pulseUntil[2]{};
        WebTimeline webTimes;
        bool attachedBefore[2]{};
        // Which of the game's two ropes draws which hand's web (one web in
        // both hands has one rope), and when a free hand last came onto the
        // other hand's web (a tick says a press there takes hold of it).
        WebRopes webRopes;
        bool onStrandBefore[2]{};
        int64_t strandTickAt[2]{};
        uint64_t zipBefore{};
        // Walls the swing took the player onto, and jumps off them, by the
        // previous frame.
        uint32_t wallsBefore{}, wallJumpsBefore{};
        // Punches each hand had landed by the previous frame.
        bool punchStarted{};
        uint64_t punchesBefore[2]{};
        // Fighting across the room (web_strike.hpp): the blows landed and the
        // web strikes opened by the previous frame; the game's time around a
        // blow; the bursts still showing where the latest blows landed, each
        // since its display time; and the sounds, made once.
        uint64_t blowsBefore{}, strikesBefore{};
        HitStop hitStop;
        // The called blow's arrow over the thug it is on (web_strike.hpp's
        // Cues), since the display time it showed.
        CueView cue{};
        uint64_t cueTarget{};
        uint32_t cueWay{};
        bool cueLocked{};
        int64_t cueSince{};
        // The player's knockouts by the previous frame, and the lettering of
        // the latest, each since its display time.
        uint64_t knockoutsBefore{};
        struct Knockout {
            KnockoutView view;
            int64_t since{};
        };
        std::array<Knockout, 2> knockoutMarks{};
        size_t nextKnockout{};
        struct Burst {
            ImpactView view;
            int64_t since{};
        };
        std::array<Burst, 6> bursts{};
        size_t nextBurst{};
        // Update 37: BLOOD SPLATTER's splashes, and the hands whose punch asks for one.
        blood_splatter::Pool bloodPool;
        blood_splatter::Visor bloodVisorDrops; // Update 38.5 EXTREME's VISOR DROPS
        uint32_t bloodPending{};
        // Update 38.1: each pending splash's ray (AIM AXIS) and, BIG, the enemy's middle and
        // feet height (tracking space; NaN: no enemy known, the fist's splash as 37).
        std::array<Vec3, 2> bloodAim{}, bloodAt{};
        std::array<float, 2> bloodFloor{NAN, NAN};
        int64_t bloodLast{};
        Vec3 bloodViewer{};
        float bloodMean{}, bloodPeak{};
        // Update 37 performance pass (perf37.hpp): the frame's sections, the overlay's parts, HOLO REUSE.
        const auto perfStart = std::chrono::steady_clock::now();
        perf37::Sections perfSections;
        perf37::HoloReuse holoReuse;
        perf37::Metric holoBuild, holoReplay, holoShown, hudPart, extrasPart, drawPart, overlayPart;
        uint64_t holoBuilt{}, holoReused{}, perfFrames{};
        // Spider-Sense's marks (spider_sense.hpp), one for each attacker the
        // game warned the player of: since the display time it showed, the
        // top of his head, what it asks for, its time left, when a counter
        // broke it, and what the sightline showed for him on the image before.
        // Then the counters made by the previous frame, and what the session
        // report counts.
        struct SenseMark {
            uint64_t attacker{};
            int64_t since{}, brokenAt{};
            Vec3 head{};
            SenseAnswer answer = SenseAnswer::Counter;
            float left = 1;
            bool timed{}, live{};
            SenseSight sight{};
        };
        std::array<SenseMark, 6> senseMarks{};
        uint64_t countersBefore{};
        uint32_t senseShown{}, senseFrames{}, senseSightFrames{}, senseTingles{};
        uint32_t senseBroken{}, senseDodges{};
        // (They outlive the session: one may still be playing as it ends.)
        static const std::vector<uint8_t> sounds[5] = {
            impactWave(ImpactSound::Light), impactWave(ImpactSound::Firm), impactWave(ImpactSound::Heavy),
            impactWave(ImpactSound::Finisher), impactWave(ImpactSound::Rush)};
        // One at a time, the newest cutting the one before short: a strike's
        // rush ends in its blow.
        auto play = [&](ImpactSound sound) {
            const auto& wave = sounds[static_cast<size_t>(sound)];
            PlaySoundW(reinterpret_cast<LPCWSTR>(wave.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
        };
        // Web-shooter shots each hand had fired by the previous frame.
        bool shooterStarted{};
        uint64_t shotsBefore[2]{};
        uint64_t submittedFrames{};
        uint64_t previousPresentedGeneration{};
        // The player's standing eye height above the floor: it rises at once
        // to a higher head and sinks a centimetre a second, so crouching
        // does not shrink the body.
        float standingEye{};
        // Each hand's fingers (HandPoser): the web-shooting hand as its web
        // button goes down and while the shot is on its way, else a fist,
        // closed by the grip, by a web held or by a hand moving fast relative
        // to the head (a punch); and where each grip was relative to the
        // head. What the swing's frame says of each hand for it: a web ball
        // left it, it holds the other hand's web (taken hold of, not shot),
        // its own web holds (attachedBefore); and an enemy is marked or
        // within a fist's reach.
        HandPoser fingers[2];
        Vec3 gripsBefore[2]{};
        bool gripsSeen[2]{};
        bool ballShot[2]{}, webTaken[2]{}, fightNear{};
        // Stretches and frames of play the game held the player on a wall
        // or a ceiling (the head stood off it).
        uint32_t surfaceEntries{};
        uint64_t surfaceFrames{};
        bool wasOnSurface{};
        // The surface the view last stood the player on, and for how long
        // since no hold has been reported: kept a moment while the game turns
        // the hero onto the wall or off it (`hold` below).
        SurfaceHold lastHold{};
        float holdGapSeconds{};
        // The player's size for the body: their T-pose calibration (the
        // launch's, or one made in this session), else the headset's running
        // height and the hero's own arms. A session with the body and without
        // a calibration asks for one at its first gameplay unless the player
        // skipped it before; the SPIDY VR tab asks again (CALIBRATE BODY: ON
        // RESUME).
        float calibratedEye = static_cast<float>(config.eyeHeightMm) / 1000;
        float calibratedArm = static_cast<float>(config.armLengthMm) / 1000;
        bool calibrationPrompt = !(config.options & 1024);
        // Reading the game's prompts and naming the VR control in them.
        const bool promptHelp = !(config.options & 0x100000);
        bool calibrationWanted = calibrationPrompt && !config.eyeHeightMm && values.body;
        body_calibration::Calibration calibration;
        body_calibration::Measurements lastCalibration{};
        // The panel's place in the tracking space, ahead of where the player
        // faced as it appeared; since when VR has shown play; when the result
        // appeared.
        Pose calibrationPanel{};
        // Version 0.0.72 TEST BUILD NOTICE: once a session after the calibration closes (test builds only).
        test_notice::Notice testNotice;
        bool testNoticeFault{};
        double testNoticeClock{}; // build 5: the notice's clock (frame.seconds is a frame's length)
        // Build 8 ROSTER page (ROSTER: HEROES AND SUITS on the POWERS page). Should it fail, only it goes off.
        roster_page::Page rosterPage;
        bool rosterFault{};
        double rosterClock{};
        uint64_t immersiveSince{}, calibrationShownAt{};
        // From the calibration's start until each is let go after it, the
        // triggers, grips, A and B do nothing in the game (B skips it).
        bool calibrationInput{}, skipHeld{};
        uint32_t calibrations{}, calibrationSkips{};
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        // Body haptics (body_haptics.hpp): a bHaptics vest, sleeves and visor,
        // played on a thread of their own (bhaptics_output.hpp), started the
        // first time BODY HAPTICS is on. What happens is posted where it
        // happens, beside the controllers' pulses (feel); what goes on, once a
        // frame. A fall in the hero's health is a blow, from where the attack
        // the game last warned of came (blowFrom, since the display time
        // blowSeen); landings come from the swing's samples.
        bhaptics::Output haptics;
        // feel: an event in its own place (a dodge's azimuth names only its
        // side); feelFrom: one that comes from a world direction, placed on the
        // vest by where the torso faces (Facing: it follows the head, the
        // room's turns at once), which the frame loop updates every frame.
        body_haptics::Facing facing;
        body_haptics::Body torso;
        const auto feel = [&](body_haptics::Kind kind, int hand = -1, float strength = 1, float azimuth = 0) {
            haptics.post({kind, static_cast<int8_t>(hand), strength, azimuth, .4f, false});
        };
        const auto feelFrom = [&](body_haptics::Kind kind, int hand, float strength, Vec3 from) {
            const bool placed = finite(from) && length(from) > 1e-3f;
            haptics.post({kind, static_cast<int8_t>(hand), strength, placed ? torso.azimuth(from) : 0.f,
                          placed ? torso.height(from) : .4f, placed});
        };
        // The groups the tab's WEB, WIND, COMBAT and MOVEMENT HAPTICS switches
        // leave on (body_haptics::State::groups).
        const auto hapticGroups = [&] {
            using body_haptics::Group;
            const auto bit = [](bool on, Group g) { return on ? 1u << static_cast<unsigned>(g) : 0u; };
            return bit(values.hapticWebs, Group::webs) | bit(values.hapticWind, Group::wind) |
                   bit(values.hapticCombat, Group::combat) | bit(values.hapticMovement, Group::movement);
        };
        body_haptics::LandingWatch landingWatch;
        body_haptics::HealthWatch heroHealth;
        // Update 36: the share of his most hit points he has, for the wrist
        // ring (NaN while unread: the ring says N/A).
        float heroShare = std::numeric_limits<float>::quiet_NaN();
        // ...and the share the ring shows, easing a hit's drop (health_ring::Eased).
        health_ring::Eased heroShown;
        Vec3 blowFrom{};
        int64_t blowSeen{};
        uint32_t hits{}, aimedHits{}, landings{};
        bhaptics::Status hapticsTold{};
        uint32_t devicesTold{};
        uint64_t hapticsSince{};
        // Slow motion: the left thumbstick's click (StickPress keeps it apart
        // from clicking both), its focus and the game's eased time
        // (SlowMotion). Each step sets the game's time, gives the controllers
        // a pulse at a change and reports it; without headset frames for a
        // while the loop below steps it to its end.
        SlowMotion slowMotion;
        // And the moments it slows by itself (a finisher, a called blow, a
        // thug pulled in the air, a perfect dodge).
        SlowMoments moments;
        StickPress slowPress;
        SlowMotionData slowCounts;
        uint64_t slowSteppedMs = GetTickCount64();
        auto stepSlowMotion = [&](float seconds, bool pressed, bool allowed) {
            using Event = SlowMotion::Event;
            const bool before = slowMotion.active();
            const auto event = slowMotion.update(seconds, pressed, allowed);
            // A landed blow stops the game's time for a moment, in slow motion too.
            hitStop.update(seconds);
            if (!allowed)
                hitStop.reset();
            moments.update(seconds, allowed);
            game_time::slow(std::min({slowMotion.timeScale(), hitStop.timeScale(), moments.timeScale()}));
            slowSteppedMs = GetTickCount64();
            if (before && std::isfinite(seconds))
                slowCounts.seconds += std::clamp(seconds, 0.f, .1f);
            switch (event) {
            case Event::started:
                ++slowCounts.starts;
                runtime.haptic(0, .55f);
                runtime.haptic(1, .55f);
                feel(body_haptics::Kind::slowStart);
                break;
            case Event::stopped:
                ++slowCounts.stops;
                runtime.haptic(0, .3f);
                runtime.haptic(1, .3f);
                feel(body_haptics::Kind::slowStop);
                break;
            case Event::emptied:
                ++slowCounts.emptied;
                runtime.haptic(0, .8f);
                feel(body_haptics::Kind::slowStop);
                break;
            case Event::refused:
                ++slowCounts.refused;
                runtime.haptic(0, .6f);
                break;
            case Event::interrupted:
                ++slowCounts.interrupted;
                break;
            case Event::none:
                break;
            }
            const auto time = game_time::telemetry();
            std::lock_guard lock(telemetry);
            auto& d = SpidySlowMotionData;
            InterlockedIncrement64(&d.sequence);
            d.starts = slowCounts.starts;
            d.stops = slowCounts.stops;
            d.refused = slowCounts.refused;
            d.emptied = slowCounts.emptied;
            d.interrupted = slowCounts.interrupted;
            d.active = slowMotion.active();
            d.seconds = slowCounts.seconds;
            d.focus = slowMotion.focus();
            d.blend = slowMotion.blend();
            d.wanted = slowMotion.timeScale();
            d.gameScale = time.own;
            d.worldScale = time.world;
            d.physicsStep = time.physicsStep;
            d.physicsScaled = time.physicsScaled;
            d.updates = time.updates;
            d.slowedUpdates = time.slowed;
            for (unsigned i = 0; i < std::size(d.moments); ++i)
                d.moments[i] = moments.count(static_cast<Moment>(i));
            d.momentActive = moments.active();
            d.momentScale = moments.timeScale();
            InterlockedIncrement64(&d.sequence);
        };
        // Puts the settings into effect: at once, or when the module they
        // belong to starts (each start reads `values`).
        // What the swing is told during play: the settings, a hand-over's
        // walls, and the hands that land no blow: the phone's while it is out,
        // and both while the game names its X, whose press a thrown fist is
        // then (NamedPress) - a blow of Spidy's on top would hit as well.
        auto sendSwingSettings = [&] {
            if (!swingStarted || !swingSettings)
                return;
            game_swing::Settings s;
            s.grab = values.webGrab && sampleGrab;
            s.maxSpeed = values.swingSpeed;
            s.airWebs = values.airWebs;
            s.gravity = vr_settings::gravity(values.weight);
            // Update 37: wall_crawl 1 leaves walls to the game's own crawl (the body flat
            // on the wall, its legs moving: CANDIDATE), 0 the swing's wall run (plan B).
            s.walls = !(config.options & 16384) && !gameWallCrawl.load();
            s.arcade = values.arcadeWebs;
            s.handClimb = values.handClimb;
            sentSwingSettings = s;
            swingSettingsSent = true;
            // A hand-over owns the walls while it lasts; the player's own
            // setting is kept above and goes back when it ends.
            if (wallsHandedBack)
                s.walls = 0;
            phoneBlows = phone.live();
            fistsBlows = namedFists;
            s.noBlows = phoneBlows || fistsBlows ? 3u : 0u;
            if (const auto code = swingSettings(&s)) {
                const auto text = "VR settings: the swing did not take them (" + std::to_string(code) + ")";
                message(3, 0, text.c_str());
            }
        };
        auto applySettings = [&] {
            runtime.hapticStrength(static_cast<float>(values.haptics) / 100);
            // The bHaptics library loads, on the haptics' own thread, the first
            // time BODY HAPTICS is on; at 0 the devices go quiet.
            if (values.bodyHaptics > 0 && !haptics.started()) {
                bhaptics::Config felt;
                felt.library = bhaptics::libraryBeside();
                felt.appId = bhaptics::builtAppId();
                felt.apiKey = bhaptics::builtApiKey();
                haptics.start(felt);
                hapticsSince = GetTickCount64();
            }
            haptics.level(values.bodyHaptics);
            rig.snapTurn(static_cast<float>(values.snapTurn) * 3.14159265f / 180);
            rig.smoothTurn(static_cast<float>(values.smoothTurn) * 3.14159265f / 180);
            rig.flips(values.flips);
            rig.flipSpeed(static_cast<float>(values.flipSpeed) * 3.14159265f / 180);
            rig.triggerWebs(values.triggerWebs);
            rig.standOnWalls(values.standOnWalls);
            rig.handClimb(values.handClimb);
            rig.wallDistance(static_cast<float>(values.wallDistance) / 100.f);
            rig.automaticFlips(realisticFlips(values));
            native_hud::setSize(values.hud);
            near_clip::set(values.nearClipMm);
            recording_fov::set(values.recordingFov);
            sendSwingSettings();
            if (swingStarted && startPunch && samplePunch) {
                if (values.punch && !punchStarted) {
                    game_punch::Config punch;
                    punch.pid = config.pid;
                    punch.base = config.base;
                    punchStarted = !startPunch(&punch);
                } else if (!values.punch && punchStarted && stopPunch) {
                    stopPunch(nullptr);
                    punchStarted = false;
                }
            }
            if (swingStarted && startShooter && sampleShooter && stopShooter) {
                if (values.webShooter && !shooterStarted) {
                    game_shooter::Config shooter;
                    shooter.pid = config.pid;
                    shooter.base = config.base;
                    shooterStarted = !startShooter(&shooter);
                } else if (!values.webShooter && shooterStarted) {
                    stopShooter(nullptr);
                    shooterStarted = false;
                }
            }
            // Turned off, the body blends back to the game's pose (Command
            // flags); it is started only once.
            // Version 0.0.72 GAME BODY (EXPERIMENTAL): on, the body waits for the strict
            // checks (native_game_body.hpp) and is started from the frame loop once they pass;
            // off, everything is as before (the checks' thread stopped).
            const bool strictBody = c38(combat38::gameBody) == 1;
            if (nativeStarted && values.body && !strictBody && !bodyStarted)
                bodyStarted = !native_body::start(config.base);
        };
        applySettings();
        auto controls = [&](uint32_t keys) {
            bridge::Control c;
            c.modes = bridge::Mode::input;
            c.serial = ++serial;
            c.leaseMs = 250;
            c.keys = keys;
            check(submitInput(&c), "Controller input");
        };
        // The game replaced its player or has none: every module that acts on
        // the player follows. A player gone again before this reaches the
        // modules is reported by the watch once more.
        auto attach = [&](const game_player::Player& next) {
            const auto follow = [&](const game_player::Player& p) {
                bridge::Config target;
                target.pid = config.pid;
                target.imageBase = config.base;
                target.hero = p.hero;
                target.actorRecord = p.record;
                if (retargetBridge(&target))
                    return false;
                if (!swingStarted)
                    return true;
                game_swing::Config swing;
                swing.pid = config.pid;
                swing.base = config.base;
                swing.record = p.record;
                swing.mover = p.mover;
                return !retargetSwing(&swing);
            };
            const uint64_t heroBefore = player.hero ? player.hero : lastAttachedHero;
            player = follow(next) ? next : game_player::Player{};
            if (!player.hero)
                follow({});
            // Update 38.5: another hero (Peter <-> Miles): the mirror learns his suit afresh.
            if (player.hero) {
                if (heroBefore && heroBefore != player.hero)
                    suitColours.forget();
                lastAttachedHero = player.hero;
            }
            native_appearance::setPlayerRecord(player.record);
            native_body::retarget(player.record);
            if (websStarted)
                native_webs::retarget(player.record);
            playersFound += player.hero != 0;
            // A respawn or another hero: his health is his own.
            heroHealth.reset();
            heroShare = std::numeric_limits<float>::quiet_NaN();
            heroShareWas = heroShare; // Update 38: rig.reset() levels the view
            // The tracking space aligns with the new body's camera again.
            rig.reset();
            history.clear();
        };
        bool headsetLost{}, restartTold{};
        unsigned recoveriesSeen{};
        uint64_t stallToldFor = ~0ull;
        while (!stopRequested && (!nativeDeadline || GetTickCount64() < nativeDeadline)) {
            if (!config.durationMs && GetTickCount64() >= keepAliveDeadline)
                break;
            double copyMs{}, overlayMs{};
            Presentation drawn{};
            // This frame shows the game's presented frame on the screen, or
            // (stereoScene) the scene's own two eye images once there are some.
            bool showScreen{}, stereoScene{};
            if (!steamGuarded && GetTickCount64() >= steamRetryMs) {
                const uint32_t code = game_steam::install(config.base);
                steamGuarded = !code;
                steamRetryMs = GetTickCount64() + 2000;
                if (code && code != 9801 && !steamWarned) {
                    steamWarned = true;
                    const auto text = "Quitting to the game's main menu may close the game: its Steam ticket "
                                      "call could not be guarded (" + std::to_string(code) + ")";
                    message(3, 0, text.c_str());
                }
            }
            if (!padInstalled && GetTickCount64() >= padRetryMs) {
                padInstalled = !game_pad::install();
                padRetryMs = GetTickCount64() + 1000;
            }
            // VR play has eye images; the game played on its screen has the
            // screen's, which these do not count.
            const bool eyesWanted = motion.active && !flatScreen;
            // Update 39.0.3: the headset lost (XrRuntime::lost) never ends VR: the game stays in VR mode and
            // VR comes back by itself once the headset does.
            if (runtime.lost() != headsetLost) {
                headsetLost = runtime.lost();
                message(3, 0, headsetLost ? "Headset lost: reopen Virtual Desktop in your headset; VR comes back by itself"
                                          : "Headset back: VR goes on");
            }
            if (headsetLost && runtime.restartNeeded() && !restartTold) {
                restartTold = true;
                message(3, 0, "The headset came back with smaller eyes: restart the game to play in VR again");
            }
            if (runtime.recoveries() != recoveriesSeen) {
                // A new session: its first images are waited for afresh.
                recoveriesSeen = runtime.recoveries();
                lastPresentedMs = lastNewImageMs = 0;
                firstImageDeadline = GetTickCount64() + 3000;
            }
            // Update 39.0.3: eye images that stop (or never start) are said, once each time, and VR goes on;
            // they used to end VR with the game on its flat screen.
            if (eyesWanted && !headsetLost &&
                ((firstImageDeadline && !lastNewImageMs && GetTickCount64() >= firstImageDeadline) ||
                 (lastNewImageMs && GetTickCount64() - lastNewImageMs >= 3000)) &&
                stallToldFor != lastNewImageMs) {
                stallToldFor = lastNewImageMs;
                message(3, 0, "The game's eye images paused: VR goes on and shows them when they come back");
            }
            const bool active = runtime.frameStereo(
                [&](const XrFrame& trackedFrame) {
                    perfSections.begin(); // Update 37 (perf37.hpp): every way out ends it
                    perf37::Guard perfGuard{perfSections,
                                            std::chrono::duration<double>(std::chrono::steady_clock::now() - perfStart).count()};
                    XrFrame frame = trackedFrame;
                    // Build 6: the TEST BUILD NOTICE closes on A (either controller: the left
                    // one's A is its X). Its raw A is kept for it, and while the notice shows,
                    // or the A that closed it is still held, A / X reach nothing else: the
                    // closing press is never a jump (nor a charged leap on its release).
                    const bool noticeA[2]{(frame.buttons & buttonX) != 0, frame.jump};
                    // Build 8 ROSTER page: it reads the raw hands (laser + trigger / A, or a poke; B / Y back),
                    // and while it is open, or a press it saw is still held, the triggers, A / B / X / Y reach
                    // nothing else (no web, no jump, no wrist press under it).
                    if (!rosterFault && (rosterPage.holdsInput() || rosterPage.opacity() > 0)) { // and its fade out
                        try {
                            rosterClock += std::isfinite(frame.seconds) ? std::clamp(static_cast<double>(frame.seconds), 0.0, .1) : 0.0;
                            roster_page::Input rin;
                            rin.seconds = rosterClock;
                            rin.head = validTrackedPose(frame.head) ? frame.head : Pose{{NAN, 0, 0}, {}};
                            rin.playing = static_cast<uint32_t>(wrist_hud::activeHero());
                            for (unsigned i = 0; i < 2; ++i) {
                                const auto& hand = frame.hands[i];
                                rin.hands[i].tracked = hand.valid && validTrackedPose(hand.grip);
                                rin.hands[i].grip = hand.grip;
                                rin.hands[i].aim = hand.aim;
                                rin.hands[i].trigger = hand.trigger;
                                rin.hands[i].a = noticeA[i];
                                rin.hands[i].b = (frame.buttons & (i ? buttonB : buttonY)) != 0;
                            }
                            const uint32_t pressesBefore = rosterPage.presses();
                            const auto act = rosterPage.update(rin);
                            if (rosterPage.presses() != pressesBefore)
                                runtime.haptic(0, .12f), runtime.haptic(1, .12f);
                            if (act.playAs >= 0 && act.playAs < static_cast<int>(hero_modes::heroes)) {
                                if (act.playAs == 0)
                                    for (const auto o : combat38::heroToggles)
                                        combat38Values[o] = 0;
                                else {
                                    const auto o = combat38::heroToggles[act.playAs - 1];
                                    combat38Values[o] = 1;
                                    heroOnly(o);
                                }
                                consoleLine(std::string("ROSTER: PLAY AS ") + wrist_hud::activeHeroName(act.playAs));
                            }
                            if (act.suitHero >= 0 && act.suitIndex >= 0) {
                                rosterSuitRequest = static_cast<uint32_t>(act.suitHero) * 16 + static_cast<uint32_t>(act.suitIndex) + 1;
                                consoleLine("ROSTER: suit " + std::to_string(act.suitIndex) + " of hero " + std::to_string(act.suitHero) + " picked");
                            }
                            if (act.closed)
                                consoleLine("ROSTER: closed");
                        } catch (...) {
                            rosterFault = true;
                            rosterPage.close();
                            consoleLine("ROSTER: off for this session (a fault)");
                        }
                        if (!rosterFault && rosterPage.holdsInput()) {
                            frame.jump = false;
                            frame.buttons &= ~static_cast<uint32_t>(buttonA | buttonB | buttonX | buttonY);
                            for (auto& hand : frame.hands)
                                hand.trigger = 0;
                        }
                    }
                    if (test_notice::testBuild && !testNoticeFault && testNotice.holdsA()) {
                        frame.jump = false;
                        frame.buttons &= ~static_cast<uint32_t>(buttonA | buttonX);
                    }
                    // The phone takes its controls first (Phone::take, PhoneHold):
                    // while it is live the stick picks a button, A presses it, B
                    // puts it away and the triggers step the volume, and none of
                    // its controls reaches the swing or the game.
                    {
                        const auto use = phone.take(frame, phoneShown.gearCount);
                        const auto& e = use.controls;
                        if (e.x || e.y)
                            runtime.haptic(use.stick, .08f);
                        if (e.volume) {
                            phoneMusic.setVolume(phoneMusic.status().volume + e.volume * phoneVolumeStep);
                            runtime.haptic(e.volume < 0 ? 0 : 1, .1f);
                        }
                        switch (use.action) {
                        case PhoneAction::previous:
                            phoneMusic.previous();
                            break;
                        case PhoneAction::play:
                            // Update 19: one music at a time; the phone's pauses the page's.
                            if (!phoneMusic.status().playing && musicStatus.playing)
                                musicPlayer.toggle();
                            phoneMusic.toggle();
                            break;
                        case PhoneAction::next:
                            phoneMusic.next();
                            break;
                        case PhoneAction::autoplay:
                            phoneMusic.setAutoplay(!phoneMusic.status().autoplay);
                            break;
                        case PhoneAction::none:
                        case PhoneAction::close:
                            break;
                        }
                        if (e.pressed || (e.closed && use.action != PhoneAction::close))
                            runtime.haptic(1, .2f);
                        if (use.action == PhoneAction::close) {
                            runtime.haptic(1, .15f);
                            message(3, 0, "Phone away - each control is the game's again once let go of");
                        }
                    }
                    uint64_t changes{};
                    if (const auto found = players.current(changes); changes != playerChanges) {
                        playerChanges = changes;
                        attach(found);
                    }
                    bridge::Data game{};
                    check(sampleBridge(&game), "Gameplay sample");
                    LARGE_INTEGER now{};
                    QueryPerformanceCounter(&now);
                    Mat4 body{}, camera{};
                    std::memcpy(body.data(), game.player, 64);
                    std::memcpy(camera.data(), game.before, 64);
                    // Gameplay: the player's own follow or combat camera
                    // committed within 100 ms (game_bridge.cpp).
                    const bool bridgeRunning = game.state == 1;
                    const bool committed = game.qpc && now.QuadPart >= static_cast<int64_t>(game.qpc) &&
                                           static_cast<double>(now.QuadPart - game.qpc) / frequency.QuadPart < .1;
                    const bool followsPlayer = native_view::validPose(body) && native_view::validPose(camera);
                    const bool gameplay = player.hero && bridgeRunning && committed && followsPlayer;
                    uint64_t moverTable{};
                    read(game.mover, &moverTable, sizeof(moverTable));
                    // The headset is worn, focused and tracked: an image can be shown.
                    const bool viewing = frame.focused && frame.valid;
                    displayTime = frame.predictedDisplayTime;
                    // The game as it is, on its screen, instead of VR: the
                    // SPIDY VR tab's VIEW (a click of both thumbsticks until
                    // October 12). Its gameplay is the game screen's, in 3D
                    // as scenes are there, the controllers an Xbox
                    // controller's, and nothing of Spidy's on the hero.
                    // Update 38.5 PLAY ON MONITOR: the pause menu's VIEW, the holo's confirmed press
                    // and F8 (the game's window in front) all move one wanted state (extras385::view).
                    // Live, no restart: the XR session stays; VR comes back the same way.
                    extrasClock += std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                    {
                        int request = -1;
                        if (monitorPressed) {
                            monitorPressed = false;
                            if (monitorConfirm.press(static_cast<float>(extrasClock)))
                                request = 1;
                            else
                                message(3, 0, "PLAY ON MONITOR - press again within 4 s; F8 or the pause menu "
                                              "brings VR back");
                        }
                        const HWND front = GetForegroundWindow();
                        DWORD frontPid{};
                        if (front)
                            GetWindowThreadProcessId(front, &frontPid);
                        const bool f8 = frontPid == GetCurrentProcessId() && (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
                        viewSwitch.update(frame.seconds, values.flatScreen, request, f8);
                    }
                    if (viewSwitch.wanted() != flatScreen && viewing) {
                        flatScreen = viewSwitch.wanted();
                        values.flatScreen = flatScreen; // the pause menu names the way back (SWITCH TO VR)
                        message(3, 0, flatScreen ? "Playing on the monitor - press F8 or the pause menu's SWITCH TO VR "
                                                   "to come back"
                                                 : "Back in VR");
                        rig.reset();
                        history.clear();
                        lastPresentedMs = lastNewImageMs = 0;
                        presented = false;
                        firstImageDeadline = GetTickCount64() + 3000;
                        // Chosen in the pause menu, the game's screen is the
                        // menu's own, where it stands.
                        if (!wasScreen)
                            screenPose = screenAhead(frame.head);
                        std::lock_guard lock(telemetry);
                        InterlockedIncrement64(&SpidyXrData.sequence);
                        ++SpidyXrData.toggles;
                        SpidyXrData.flatScreen = flatScreen;
                        InterlockedIncrement64(&SpidyXrData.sequence);
                    }
                    if (!frame.jump)
                        jumpFromScreen = false;
                    else if (wasScreen)
                        jumpFromScreen = true;
                    XrFrame controller = frame;
                    // The T-pose calibration has A as it has the other buttons.
                    controller.jump = frame.jump && !jumpFromScreen && !calibrationInput;
                    // Each hand's grip pose, and whether the headset has it; and
                    // whether the hand gestures play VR: gameplay in the headset,
                    // the controllers no calibration's.
                    const Pose grips[2]{frame.hands[0].grip, frame.hands[1].grip};
                    const bool gripsTracked[2]{frame.hands[0].valid && validTrackedPose(frame.hands[0].grip),
                                               frame.hands[1].valid && validTrackedPose(frame.hands[1].grip)};
                    const bool gesturesPlay = viewing && gameplay && !flatScreen && !calibrationInput &&
                                              validTrackedPose(frame.head);
                    {   // Update 38.5 SYMBIOTE LOOK: both hands grab the chest, then pull outward - on or
                        // off (extras385::symbiote::ChestPull; the ULTIMATE's crossed arms never arm it).
                        gestures385::Input gi;
                        gi.seconds = frame.seconds;
                        gi.play = gesturesPlay && !padLayer.latched() && !phone.open() && !(wristToggle.opacity() > 0);
                        gi.head = frame.head.position;
                        gi.forward = frame.head.orientation.rotate(Vec3{0, 0, -1});
                        for (unsigned i = 0; i < 2; ++i)
                            gi.hands[i] = {gripsTracked[i], grips[i].position, frame.hands[i].trigger,
                                           frame.hands[i].squeeze};
                        gi.xHeld = (frame.buttons & buttonX) != 0;
                        gi.webAttached = swingState.webs[0].attached != 0 || swingState.webs[1].attached != 0;
                        if (chestPull.update(gi)) {
                            symbioteOn = !symbioteOn.load();
                            runtime.haptic(0, .8f);
                            runtime.haptic(1, .8f);
                            // Version 0.0.3 (rep0022: the gesture WAS seen, twice; the user expected the suit
                            // to change): say plainly that it is the mod's look, and log the suit id probe.
                            message(3, 0, symbioteOn.load() ? "SYMBIOTE LOOK on (mod gloves, webs, mirror) - the game suit "
                                                              "stays: pick suits in SUITS; pull again for off"
                                                            : "SYMBIOTE LOOK off (the game suit was never changed)");
                            char line[240];
                            sprintf_s(line,
                                      "Version 0.0.3 SYMBIOTE gesture: detected (chest grab, pull apart) -> SYMBIOTE LOOK %s; "
                                      "the game's suit NOT switched (no confirmed suit-change path); suit id now 0x%x "
                                      "(hero + 0x8c, CANDIDATE, %s)",
                                      symbioteOn.load() ? "on" : "off", suitWatch.now, suitWatch.have ? "read" : "not read yet");
                            game_prompts::note(line);
                        }
                        symbioteSpread.step(frame.seconds, symbioteOn.load() && !flatScreen);
                        // Update 38.5 FLY's poses: live with FLY MODE on (or GESTURE TRAINING, which only
                        // names them), in play, X not held, no hologram, pad layer or phone.
                        try {
                            const bool flyTraining = gestureMovesNow.load() == gestures385::modeTraining;
                            extras385::fly::PoseInput pi;
                            pi.seconds = frame.seconds;
                            pi.live = gi.play && !gi.xHeld && (flyModeNow.load() != 0 || flyTraining);
                            pi.head = gi.head;
                            pi.forward = gi.forward;
                            for (unsigned i = 0; i < 2; ++i) {
                                pi.tracked[i] = gripsTracked[i];
                                pi.hands[i] = grips[i].position;
                            }
                            // Build 7 THOR: the hammer hand never makes RISE (ONE HAND UP) - raising the hammer is
                            // THOR STORM and a throw swings it high; build 6 took off instead. The left hand up rises.
                            if (heroNow() == hero_modes::thor && hero072Play.hammerInHandOrOut() && finite(pi.head) &&
                                finite(pi.hands[1]))
                                pi.hands[1].y = std::min(pi.hands[1].y, pi.head.y - .15f);
                            flyPoses.update(pi);
                            glPose = flyPoses.pose();
                            glLift = flyPoses.lift();
                            if (heroNow() == hero_modes::greenLantern && c38(combat38::glFlight) == 0 && pi.live &&
                                pi.tracked[0] && pi.tracked[1])
                                glPose = glRingFlight.gate(frame.seconds, flyNow.flying != 0, flyPoses.pose(), flyPoses.lift(),
                                                           pi.head, pi.hands[0], pi.hands[1], glLift);
                            const uint32_t stage = flyPoses.stage(), shown = flyPoses.shown();
                            if (flyTraining && pi.live && (stage != flyStageShown || shown != flyPoseShown) && stage) {
                                char line[112];
                                sprintf_s(line, "FLY %s - %s", extras385::fly::poseName(flyPoses.shown()),
                                          stage == 1 ? "RED, keep holding" : stage == 2 ? "BLUE, nearly" : "SUCCESS");
                                message(3, 0, line);
                            }
                            flyStageShown = stage;
                            flyPoseShown = shown;
                        } catch (...) {
                            flyPoses.reset();
                            flyModeNow = 0;
                            game_prompts::note("Update 38.5: FLY's poses failed and FLY MODE turned itself off");
                        }
                        // Update 38.5 GESTURE MOVES: TRAINING names each move (nothing sent); ON queues
                        // the move's pad presses (sent just before the pad goes out) and the wings steer.
                        const uint32_t gestureMode = gestureMovesNow.load();
                        gestureOut = gestures385::Out{};
                        gesturePlayNow = gi.play && gestureMode != gestures385::modeOff;
                        if (gesturePlayNow) {
                            gi.airborne = gestureInFlight;
                            gi.holoBusy = wristToggle.opacity() > 0;
                            gi.stick = std::min(1.f, std::hypot(frame.hands[0].stickX, frame.hands[0].stickY));
                            gestureRec.trainingFails_ = gestureMode == gestures385::modeTraining;
                            gestureSet.wingArms = flyModeNow.load() == 0; // FLY MODE's arms are its own
                            try {
                                gestureOut = gestureRec.update(gi, gestureSet);
                            } catch (...) {
                                gestureOut = gestures385::Out{};
                                gestureMovesNow = gestures385::modeOff;
                                game_prompts::note("Update 38.5: GESTURE MOVES failed and turned itself off");
                            }
                            for (int e = 0; e < gestureOut.eventCount && e < gestures385::Out::events; ++e) {
                                const auto& ev = gestureOut.event[e];
                                if (ev.gesture >= gestures385::count)
                                    continue;
                                // GESTURE REMAP: a move with the player's own motion ignores the built-in
                                // one; nothing built-in while a move is being recorded or asked about.
                                if (!remapFault && (gestureRemap.busy() || gestureRemap.hasCustom(ev.gesture)))
                                    continue;
                                if (gestureMode == gestures385::modeTraining && ev.gesture == trainGuide.move())
                                    trainGuide.result(ev.success, ev.reason); // Version 0.0.2 the guide's verdict
                                char line[96];
                                if (ev.success)
                                    sprintf_s(line, "%s - OK", gestures385::gestureName(ev.gesture));
                                else
                                    sprintf_s(line, "%s - FAIL: %s", gestures385::gestureName(ev.gesture), ev.reason);
                                if (gestureMode == gestures385::modeTraining) {
                                    message(3, 0, line);
                                } else if (ev.success && !gestures385::isGadget(ev.gesture) &&
                                           comboSender.push(gestureSet.input[ev.gesture])) {
                                    runtime.haptic(ev.gesture == gestures385::parry ? 0 : 1, .4f);
                                }
                            }
                        } else {
                            comboSender.clear();
                        }
                        // Update 38.5 GESTURE REMAP: recording takes the motions with the hologram open
                        // (the trail shows there); a saved motion plays only with GESTURE MOVES ON
                        // (TRAINING names it), never with the hologram, phone or pad layer up.
                        if (!remapFault) {
                            try {
                                gesture_remap::Capture::Frame rf;
                                const bool remapPlay = gesturesPlay && !padLayer.latched() && !phone.open();
                                rf.seconds = remapPlay && std::isfinite(frame.seconds) ? frame.seconds : 0.f;
                                rf.head = frame.head.position;
                                rf.look = frame.head.orientation.rotate(Vec3{0, 0, -1});
                                for (unsigned i = 0; i < 2; ++i) {
                                    rf.hand[i] = grips[i].position;
                                    rf.tracked[i] = gripsTracked[i];
                                    rf.buttons[i] = static_cast<uint8_t>(
                                        (frame.hands[i].trigger > .5f ? gesture_remap::trigger : 0) |
                                        (frame.hands[i].squeeze > .5f ? gesture_remap::grip : 0));
                                }
                                if (frame.buttons & buttonX)
                                    rf.buttons[0] = static_cast<uint8_t>(rf.buttons[0] | gesture_remap::xButton);
                                const float arm = std::isfinite(calibratedArm) && calibratedArm > .3f && calibratedArm < 1.2f
                                                      ? calibratedArm
                                                      : .6f;
                                const bool wasBusy = gestureRemap.busy();
                                const int fired = gestureRemap.update(rf, arm, wristToggle.opacity() > 0,
                                                                      gi.play && gestureMode != gestures385::modeOff);
                                if (wasBusy && !gestureRemap.busy())
                                    trainReselect = true; // Version 0.0.2: a new recording is the guide's path
                                if (wasBusy && !gestureRemap.busy() && gestureRemap.view().step ==
                                                                           static_cast<uint8_t>(gesture_remap::Step::saved)) {
                                    char line[96];
                                    sprintf_s(line, "%s - YOUR MOVE SAVED",
                                              gestures385::gestureName(gestureRemap.view().move));
                                    message(3, 0, line);
                                }
                                if (fired >= 0 && static_cast<uint32_t>(fired) < gestures385::count) {
                                    char line[96];
                                    sprintf_s(line, "%s (MINE) - OK", gestures385::gestureName(static_cast<uint32_t>(fired)));
                                    if (gestureMode == gestures385::modeTraining && fired == trainGuide.move())
                                        trainGuide.result(true, "");
                                    if (gestureMode == gestures385::modeTraining)
                                        message(3, 0, line);
                                    else if (gestureMode == gestures385::modeOn &&
                                             comboSender.push(gesture_session::inputOf(static_cast<uint32_t>(fired),
                                                                                       gestureSet.input)))
                                        runtime.haptic(1, .4f);
                                }
                                if (gestureRemap.dirty() && !gestureRemap.busy()) {
                                    if (!writeWhole(gesturesPath(), gestureRemap.text()))
                                        message(3, 0, "GESTURE REMAP: could not save your moves (they last until the game closes)");
                                }
                                remapView = gestureRemap.view();
                            } catch (...) {
                                remapFault = true;
                                remapView = gesture_session::View{};
                                game_prompts::note("Update 38.5: GESTURE REMAP failed and turned itself off; the built-in moves play");
                            }
                        }
                        // Version 0.0.2 TRAINING's guide: the picked move's path from the head, scaled to the
                        // arm; RED -> BLUE as the hand follows it -> SUCCESS / FAIL (the recogniser's verdict).
                        trainDraw = training_guide::Draw{};
                        if (!trainGuideFault) {
                            try {
                                const int pick = std::clamp(trainMovePicked, 0, training_guide::moves - 1);
                                const bool mine = pick < static_cast<int>(gestures385::count) && !remapFault &&
                                                  gestureRemap.hasCustom(static_cast<uint32_t>(pick));
                                if (trainReselect || pick != trainGuide.move() || mine != trainMine) {
                                    trainGuide.select(pick, training_guide::pathFor(
                                                                pick, mine ? &gestureRemap.moves()[pick] : nullptr));
                                    trainMine = mine;
                                    trainReselect = false;
                                }
                                training_guide::Input ti;
                                ti.seconds = frame.seconds;
                                ti.live = gestureMode == gestures385::modeTraining && gi.play;
                                ti.head = frame.head.position;
                                ti.look = gi.forward;
                                for (unsigned i = 0; i < 2; ++i) {
                                    ti.tracked[i] = gripsTracked[i];
                                    ti.hands[i] = grips[i].position;
                                }
                                ti.arm = std::isfinite(calibratedArm) && calibratedArm > .3f && calibratedArm < 1.2f
                                             ? calibratedArm
                                             : .6f;
                                using extras385::fly::Pose;
                                const Pose want = pick == training_guide::flyRise      ? extras385::fly::rise
                                                  : pick == training_guide::flyForward ? extras385::fly::forward
                                                  : pick == training_guide::flyStop    ? extras385::fly::brake
                                                                                       : extras385::fly::hover;
                                ti.poseEngaged = training_guide::isPose(pick) && want != extras385::fly::hover &&
                                                 flyPoses.pose() == want;
                                const auto before = trainGuide.state();
                                trainGuide.update(ti);
                                const auto stateNow = trainGuide.state();
                                if (stateNow != before && stateNow == training_guide::success) {
                                    ++trainSuccesses;
                                    runtime.haptic(0, .5f);
                                    runtime.haptic(1, .5f);
                                } else if (stateNow != before && stateNow == training_guide::fail) {
                                    ++trainFails;
                                    runtime.haptic(1, .2f);
                                }
                                if (trainGuide.on()) {
                                    trainDraw.on = true;
                                    trainDraw.state = static_cast<uint8_t>(stateNow);
                                    trainDraw.pose = trainGuide.path().pose;
                                    trainDraw.head = trainGuide.path().head;
                                    trainDraw.armed = trainGuide.armed();
                                    trainDraw.hands = trainGuide.path().hands;
                                    trainDraw.ghostAlpha = trainGuide.ghostAlpha();
                                    trainDraw.pulse = .5f + .5f * std::sin(frame.seconds * 6.f);
                                    for (int h = 0; h < 2; ++h) {
                                        trainDraw.progress[h] = trainGuide.progress(h);
                                        trainDraw.ghost[h] = trainGuide.ghost(h);
                                        for (int k = 0; k < training_guide::points; ++k)
                                            trainDraw.p[h][k] = trainGuide.point(h, k);
                                    }
                                    sprintf_s(trainLine[0], "%s%s", training_guide::moveName(pick), mine ? " (MINE)" : "");
                                    if (stateNow == training_guide::fail)
                                        sprintf_s(trainLine[1], "FAIL: %s", trainGuide.reason());
                                    else
                                        sprintf_s(trainLine[1], "%s", trainGuide.instruction());
                                }
                            } catch (...) {
                                trainGuideFault = true;
                                trainDraw = training_guide::Draw{};
                                game_prompts::note("Version 0.0.2: TRAINING's guide failed and turned itself off; TRAINING still names moves");
                            }
                        }
                    }
                    {   // Update 38.5 HERO SWITCH PROBE: read-only; the console line says how the switch went.
                        const uint64_t solved = native_body::status().solved;
                        const bool bodySolved = solved != bodySolvedWas;
                        bodySolvedWas = solved;
                        heroSwitch.frame(player.hero, player.record, static_cast<double>(GetTickCount64()),
                                         viewing && gameplay && !flatScreen, bodySolved, !gameplay || flatScreen);
                        const auto& hs = heroSwitch.data();
                        if (heroSwitch.swapStarted(heroSwitchSeen))
                            heroSwitchPending = 1;
                        if (heroSwitchPending && !hs.pending) {
                            heroSwitchPending = 0;
                            char line[200];
                            sprintf_s(line,
                                      "Update 38.5 HERO SWITCH: hero replaced (#%u, a switch, respawn or load); no hero "
                                      "%.0f ms; VR view back %s%.0f ms, body back %s%.0f ms",
                                      hs.swaps, hs.lastGapMs, hs.lastViewMs < 0 ? "never " : "", hs.lastViewMs,
                                      hs.lastBodyMs < 0 ? "never " : "", hs.lastBodyMs);
                            game_prompts::note(line);
                        }
                        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&SpidyHeroSwitchData.sequence));
                        const uint64_t sequence = SpidyHeroSwitchData.sequence;
                        SpidyHeroSwitchData = hs;
                        SpidyHeroSwitchData.sequence = sequence + 1;
                    }
                    // The wrist hologram (Update 14): the other wrist brought to its
                    // wrist with that hand's trigger pulled turns it on or off, and
                    // that pull is the hologram's alone - no web, reel, scan or
                    // anything of the game's - until the trigger is let go.
                    uint32_t hudTriggers{}, hudZone{};
                    {
                        const bool hudOn = wristHudOn.load() && !wristHudFault.load();
                        if (!hudOn)
                            wristToggle.set(false);
                        const float triggers[2]{frame.hands[0].trigger, frame.hands[1].trigger};
                        const bool hudPlay =
                            hudOn && gesturesPlay && !padLayer.latched() && !phone.open() && !gadgetWheel.open();
                        const unsigned hudHand = wristHudHand.load() ? 1u : 0u;
                        const unsigned pointHand = 1 - hudHand;
                        // Update 15: where the other hand points on the panel, found
                        // before the toggle so a pull on the panel never toggles it.
                        wristPointer = {};
                        const float scale = static_cast<float>(wristHudScalePercent.load()) / 100.f;
                        const float hudDt = std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                        wristClock += hudDt;
                        bool hudStick = false; // Update 17: the pointing hand's stick scrolls the panel
                        if (hudPlay && wristToggle.opacity() > 0 && gripsTracked[hudHand] &&
                            gripsTracked[pointHand]) {
                            wrist_hud::View placed;
                            placed.emitter = wrist_hud::emitter(grips[hudHand], hudHand);
                            placed.headUp = frame.head.orientation.rotate({0, 1, 0});
                            placed.scale = scale;
                            placed.heightCm = static_cast<float>(wristHudHeightCm.load());
                            wristPointer = wrist_hud::point(wrist_hud::place(placed, frame.head.position), scale,
                                                            grips[pointHand], pointHand);
                        }
                        const bool onPanel = wristPointer.hit && wrist_hud::onPanel(wristMenu.page(), wristPointer.u, wristPointer.v);
                        float toggleTriggers[2]{triggers[0], triggers[1]};
                        if (!wristMenuFault.load() && (wristMenu.claims() || (onPanel && wristToggle.opacity() > 0)))
                            toggleTriggers[pointHand] = 0;
                        // Update 38.1 HOLO OPEN (TAP / PALM LOOK / CLICK / F8 ONLY): no hands-apart guard
                        // (38's cancelled every open with the hands together, run38); the palm-up look
                        // from the head; the zone swallows the pointing hand's trigger and grip.
                        // Update 38.2 HOLO OPEN FOREARM (0, the default): the pointing hand's trigger on
                        // the hologram forearm, no palm look (Update 37's tap); 1-3 38.1's.
                        wristToggle.setMode(combat38::toggleMode(c38(combat38::holoOpen)));
                        wristToggle.look(frame.head, validTrackedPose(frame.head));
                        const float toggleSqueezes[2]{frame.hands[0].squeeze, frame.hands[1].squeeze};
                        if (wristToggle.update(frame.seconds, hudPlay, hudHand, grips, gripsTracked, toggleTriggers,
                                               toggleSqueezes)) {
                            runtime.haptic(static_cast<int>(1 - hudHand), .35f);
                            runtime.haptic(static_cast<int>(hudHand), .2f);
                        }
                        hudTriggers = wristToggle.hands();
                        hudZone = wristToggle.zoneHands();
                        // Update 20: the state the hologram's gesture saw, and F8 on the
                        // PC's keyboard (unbound in Spider-Man 2): the same toggle with
                        // no gesture, while the controllers play VR. Only a key read;
                        // nothing of the game's is touched.
                        {
                            const uint32_t state =
                                (wristHudOn.load() ? hudSettingOn : 0u) | (wristHudFault.load() ? hudFaulted : 0u) |
                                (wristMenuFault.load() ? hudMenuFaulted : 0u) | (hudPlay ? hudPlaying : 0u) |
                                (wristToggle.on() ? hudOpen : 0u) | (gesturesPlay ? hudGestures : 0u) |
                                (gameplay ? hudGameplay : 0u) | (flatScreen ? hudFlatScreen : 0u) |
                                (padLayer.latched() ? hudPadLayer : 0u) | (phone.open() ? hudPhone : 0u) |
                                (gadgetWheel.open() ? hudWheel : 0u) | (calibrationInput ? hudCalibrating : 0u) |
                                (gripsTracked[0] && gripsTracked[1] ? hudHandsTracked : 0u) |
                                (viewing ? hudViewing : 0u);
                            const bool key = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
                            if (key && !hudKeyHeld) {
                                ++hudKeyPresses;
                                if (hudPlay && wristToggle.flip()) {
                                    ++hudKeyToggles;
                                    runtime.haptic(static_cast<int>(hudHand), .2f);
                                } else {
                                    ++hudKeyRefused;
                                    hudBlocked = state;
                                }
                            }
                            hudKeyHeld = key;
                            const auto& seen = wristToggle.diagnostics();
                            if (seen.pressesNotPlaying != hudNotPlaying) {
                                hudNotPlaying = seen.pressesNotPlaying;
                                hudBlocked = state;
                            }
                            auto& t = SpidyWristHudData;
                            InterlockedIncrement64(&t.sequence);
                            t.state = state;
                            t.distance = seen.distance;
                            t.nearest = seen.nearest;
                            t.pressDistance = seen.pressDistance;
                            t.opacity = wristToggle.opacity();
                            t.presses = seen.presses;
                            t.pressesInReach = seen.pressesInReach;
                            t.pressesOutOfReach = seen.pressesOutOfReach;
                            t.pressesNotPlaying = seen.pressesNotPlaying;
                            t.pressesRearming = seen.pressesRearming;
                            t.toggles = wristToggle.toggles();
                            t.keyToggles = hudKeyToggles;
                            t.keyPresses = hudKeyPresses;
                            t.keyRefused = hudKeyRefused;
                            t.blocked = hudBlocked;
                            InterlockedIncrement64(&t.sequence);
                        }
                        // The pages (wrist_hud::Menu): a pull begun on the panel is the
                        // panel's. Values go through wristApply: clamped, a GPU-side one
                        // at most four times a second while dragged, applied here (the
                        // XR thread's frame start, before anything is submitted), a
                        // refused one put back with a red note. Should any of it throw,
                        // only the controls go off.
                        if (!wristMenuFault.load()) {
                            try {
                                const auto live = [&] {
                                    wrist_hud::Settings v;
                                    v.shadowDistance = shadowDistanceHeld ? shadowDistancePending
                                                                          : static_cast<float>(native_shadow_distance::wantCars());
                                    v.eyeTrail = static_cast<float>(native_gpu::eyeTrail()); // Update 34.2
                                    // Update 37.2: the COLOR page and INNER SIZE, as live.txt / the presses left them.
                                    v.colorStrength = static_cast<float>(colorStrength.load());
                                    v.exposure = static_cast<float>(colorExposure.load()) / 10.f;
                                    v.contrast = static_cast<float>(colorContrast.load());
                                    v.saturation = static_cast<float>(colorSaturation.load());
                                    v.vibrance = static_cast<float>(colorVibrance.load());
                                    v.warmth = static_cast<float>(colorWarmth.load());
                                    v.tint = static_cast<float>(colorTint.load());
                                    v.vignette = static_cast<float>(colorVignette.load());
                                    v.flyRollSpeed = static_cast<float>(flyRollSpeedNow.load());
                                    v.colorCartoon = static_cast<float>(colorCartoon.load());
                                    v.foveaInner = static_cast<float>(foveaInnerWant.load());
                                    v.widthCm = static_cast<float>(webRadiusTenthsMm.load()) * 2 / 100.f;
                                    v.speed = static_cast<float>(webSpeedMs.load());
                                    v.aa = static_cast<float>(native_gpu::eyeAA());
                                    v.history = static_cast<float>(native_gpu::eyeHistory());
                                    v.reach = static_cast<float>(SpidyImpostorReachNow());
                                    v.monitor = static_cast<float>(native_gpu::monitorRateNow());
                                    v.sharpen = static_cast<float>(native_gpu::eyeSharpen());
                                    v.reticles = static_cast<float>(reticlesShown.load());
                                    v.eyeTest = static_cast<float>(native_gpu::eyeDebug());
                                    v.hudHeight = static_cast<float>(wristHudHeightCm.load());
                                    v.hudScale = static_cast<float>(wristHudScalePercent.load()) / 100.f;
                                    v.hudText = static_cast<float>(wristHudTextPercent.load());
                                    v.hudTheme = static_cast<float>(wristHudTheme.load());
                                    v.hudTextColor = static_cast<float>(wristHudTextColor.load());
                                    v.webStyle = static_cast<float>(webStyleNow.load());
                                    for (int i = 0; i < web_style::count; ++i)
                                        v.webColors[i] = static_cast<float>(webShades[i].load());
                                    v.musicVolume = static_cast<float>(musicVolumeNow.load());
                                    v.musicVisualizer = static_cast<float>(musicVisualizerNow.load());
                                    const uint32_t next = nextSupersample.load();
                                    v.supersample = static_cast<float>(next ? next : native_gpu::supersample());
                                    return v;
                                }();
                                if (!wristApplyStarted) {
                                    wristApply.start(live);
                                    wristApplyStarted = true;
                                }
                                wristShown = wristApply.shown(live);
                                const bool menuActive = hudOn && hudPlay && wristToggle.opacity() > .9f &&
                                                        !(hudTriggers & (1u << pointHand)) && gripsTracked[pointHand];
                                wrist_hud::Menu::Input in;
                                in.active = menuActive;
                                in.pointer = wristPointer;
                                in.trigger = gripsTracked[pointHand] ? triggers[pointHand] : 0.f;
                                // Update 16: sharpening that could not be built is
                                // UNAVAILABLE at once (its fault, not refusals).
                                wristOff = wristApply.disabled() |
                                           (native_gpu::eyeSharpenFault()
                                                ? 1u << static_cast<unsigned>(wrist_hud::Control::sharpen)
                                                : 0u);
                                in.disabled = wristOff;
                                // Update 17: the stick scrolls a page while the pointer
                                // is on it; the applied text size and anything that
                                // moves or sizes the panel (a change: nothing pressed).
                                in.stick = gripsTracked[pointHand] ? frame.hands[pointHand].stickY : 0.f;
                                in.seconds = hudDt;
                                in.text = static_cast<float>(wristHudTextPercent.load()) / 100.f;
                                in.hero = static_cast<int>(heroNow()); // Update 39.0.2: POWERS opens this hero's profile
                                {   // Update 38.5: the drawn subtext's overflow (0: not known yet).
                                    const uint32_t ho = wristHelpOverflow.load();
                                    if (ho & 0xffu) {
                                        in.helpControl = static_cast<wrist_hud::Control>(ho >> 8);
                                        in.helpOverflow = static_cast<int>(ho & 0xffu) - 1;
                                    }
                                }
                                in.layout = std::min<uint32_t>(wristHudHeightCm.load(), 31) |
                                            std::min<uint32_t>(wristHudScalePercent.load(), 255) << 5 |
                                            std::min<uint32_t>(wristHudTextPercent.load(), 255) << 13 |
                                            hudHand << 21;
                                // Update 19: the songs, and the MUSIC page's controls
                                // off while its player is (MUSIC OFF; the volume
                                // and visualizer stay: they are only kept).
                                if (musicPlayer.status(musicStatus)) {
                                    // Update 20: for the report (SpidyWristHudData).
                                    auto& t = SpidyWristHudData;
                                    InterlockedIncrement64(&t.sequence);
                                    t.musicTracks = static_cast<uint32_t>(std::max(musicStatus.tracks, 0));
                                    t.musicTrack = musicStatus.track;
                                    t.musicState = (musicStatus.available ? 1u : 0u) | (musicStatus.playing ? 2u : 0u) |
                                                   (musicStatus.paused ? 4u : 0u) | 8u;
                                    const size_t n = std::min(musicStatus.note.size(), sizeof(t.musicNote) - 1);
                                    std::memcpy(t.musicNote, musicStatus.note.data(), n);
                                    t.musicNote[n] = 0;
                                    InterlockedIncrement64(&t.sequence);
                                }
                                {
                                    const int start = musicTrackStart.load();
                                    if (start >= 0 && musicStatus.tracks > 0) {
                                        musicPlayer.select(std::min(start, musicStatus.tracks - 1));
                                        musicTrackStart = -1;
                                    }
                                }
                                in.musicTracks = musicStatus.available ? musicStatus.tracks : 0;
                                in.musicTrack = musicStatus.track;
                                if (!musicStatus.available)
                                    for (auto c : {wrist_hud::Control::musicTrack, wrist_hud::Control::musicPrevious,
                                                   wrist_hud::Control::musicRewind, wrist_hud::Control::musicPlay,
                                                   wrist_hud::Control::musicStop, wrist_hud::Control::musicForward,
                                                   wrist_hud::Control::musicNext, wrist_hud::Control::musicSeek,
                                                   wrist_hud::Control::musicPick})
                                        in.disabled |= 1u << static_cast<unsigned>(c);
                                wristOff = in.disabled;
                                in.shadowDistanceOff = !shadow_distance::sliderMoves(native_shadow_distance::now());
                                const auto result = wristMenu.update(in, wristShown);
                                // Update 33: SHADOW DISTANCE follows the pointer while held; the
                                // search's thread is asked once it is let go (it tests, then keeps
                                // or puts back the value itself).
                                if (wristMenu.held() == wrist_hud::Control::shadowDistance && !in.shadowDistanceOff) {
                                    shadowDistancePending = wristShown.shadowDistance;
                                    shadowDistanceHeld = true;
                                } else if (shadowDistanceHeld) {
                                    shadowDistanceHeld = false;
                                    native_shadow_distance::set(static_cast<int>(std::lround(shadowDistancePending)));
                                }
                                // Update 34.2: EYE AA TRAIL is live as it is dragged (one atomic).
                                if (result.eyeTrail)
                                    native_gpu::setEyeTrail(static_cast<uint32_t>(std::lround(wristShown.eyeTrail)));
                                // The player's part: each returns at once (its thread does it).
                                {
                                    using M = wrist_hud::Menu::Music;
                                    const bool starts = result.music == M::pick || result.music == M::toggle;
                                    if (starts && !musicStatus.playing && phoneMusic.status().playing)
                                        phoneMusic.toggle(); // one music at a time: the page's pauses the phone's
                                    switch (result.music) {
                                    case M::rescan:
                                        musicPlayer.rescan();
                                        break;
                                    case M::pick:
                                        musicPlayer.play(static_cast<int>(result.musicValue));
                                        break;
                                    case M::previous:
                                        musicPlayer.previous();
                                        break;
                                    case M::rewind:
                                        musicPlayer.seekBy(-music::seekSeconds);
                                        break;
                                    case M::toggle:
                                        musicPlayer.toggle();
                                        break;
                                    case M::stop:
                                        musicPlayer.stop();
                                        break;
                                    case M::forward:
                                        musicPlayer.seekBy(music::seekSeconds);
                                        break;
                                    case M::next:
                                        musicPlayer.next();
                                        break;
                                    case M::seek:
                                        musicPlayer.seekTo(result.musicValue);
                                        break;
                                    case M::none:
                                        break;
                                    }
                                }
                                musicPlayer.setVolume(static_cast<float>(musicVolumeNow.load()));
                                // The song picked or playing, kept by the launcher (music_track).
                                if (musicStatus.available && musicStatus.track >= 0 &&
                                    musicStatus.track != musicTrackKept) {
                                    musicTrackKept = musicStatus.track;
                                    musicTrackNow = musicStatus.track;
                                    wristChanged |= 1u << static_cast<unsigned>(wrist_hud::Control::musicTrack);
                                }
                                // What the page shows (only while it is shown: the names are copied).
                                if (wristMenu.page() == wrist_hud::musicPage) {
                                    auto& m = wristMusic;
                                    m.available = musicStatus.available;
                                    m.tracks = musicStatus.tracks;
                                    m.track = musicStatus.track;
                                    m.playing = musicStatus.playing;
                                    m.paused = musicStatus.paused;
                                    m.elapsed = musicStatus.elapsed;
                                    m.total = musicStatus.total;
                                    m.song = musicStatus.song;
                                    m.note = musicStatus.note;
                                    m.folder = musicStatus.folder;
                                    m.listOpen = wristMenu.listOpen();
                                    m.listFirst = wristMenu.listFirst();
                                    m.listHover = wristMenu.listHover();
                                    m.rows.clear();
                                    if (m.listOpen)
                                        for (int i = 0; i < wrist_hud::listRows; ++i) {
                                            const auto at = static_cast<size_t>(m.listFirst + i);
                                            if (at < musicStatus.names.size())
                                                m.rows.push_back(musicStatus.names[at]);
                                        }
                                    musicPlayer.spectrum(m.spectrum);
                                }
                                hudStick = result.stick;
                                for (unsigned c = 0; c < wrist_hud::maskedControls; ++c)
                                    if (result.changed >> c & 1)
                                        wristApply.want(static_cast<wrist_hud::Control>(c),
                                                        wristShown[static_cast<wrist_hud::Control>(c)]);
                                if (result.fpsReset)
                                    wristSession.reset(); // Update 24: RESET, from the next frame
                                if (result.addressFinder) {
                                    // Update 27: press starts when idle/done/failed, stops when running.
                                    uint32_t st = 0, unused = 0;
                                    native_address_finder::viewFields(st, unused);
                                    native_address_finder::set(st == 1 ? 0u : 1u);
                                }
                                if (result.eyeAa) // Update 33: EYE AA on/off (refused once failed)
                                    pressEyeAa();
                                if (result.engineTaa) // Update 34: ENGINE TAA on/off (refused with why)
                                    pressEngineTaa();
                                if (result.taaJitter && // Update 34.2: ENGINE TAA's next jitter mode, only while it acts
                                    engine_taa::jitterActive(SpidyEngineTaaNow(), (eyeAaNow() & 0xff) == eye_taa::on))
                                    SpidyEngineTaaJitter(reinterpret_cast<void*>(uintptr_t{engine_taa::jitterModes}));
                                if (result.farShadows && // Update 35: FAR SHADOWS, the next mode; the FPS guard watches it
                                    SpidyFarShadows(reinterpret_cast<void*>(uintptr_t{far_shadows::nextMode})) == 0 &&
                                    far_shadows::modeOf(SpidyFarShadowsNow()) != far_shadows::off)
                                    farShadowsArm = true;
                                if (result.eyeBloom) // Update 36: EYE BLOOM, the next mode (it reverts on its own)
                                    SpidyEyeBloom(reinterpret_cast<void*>(uintptr_t{eye_bloom::nextMode}));
                                if (result.mirror) { // Update 37: MIRROR, show (placed in front) or dismiss
                                    const bool on = !mirrorOn.load();
                                    mirrorOn = on;
                                    if (on)
                                        mirrorRecentreAsked = true, mirrorAutoOffAtMs = 0; // ON again: the old note goes
                                }
                                if (result.blood) // Update 37: BLOOD SPLATTER, the next step (OFF -> ON -> BRUTAL -> EXTREME)
                                    bloodIntensity = blood_splatter::nextStep(bloodIntensity.load());
                                if (result.bloodVisor) // Update 38.5: EXTREME's VISOR DROPS on/off
                                    bloodVisor = !bloodVisor.load();
                                // Update 38.5 EXTRAS (extras385.hpp)
                                if (result.invincible)
                                    invincibleOn = !invincibleOn.load();
                                if (result.flyMode)
                                    flyModeNow = (flyModeNow.load() + 1) % 4;
                                if (result.flyVignette)
                                    flyVignetteOn = !flyVignetteOn.load();
                                if (result.flyRoll) // Update 38.5: FLY ROLL OFF -> COMFORT -> FULL
                                    flyRollNow = (flyRollNow.load() + 1) % extras385::fly::rollModes;
                                if (result.flyAutoLevel)
                                    flyAutoLevelNow = !flyAutoLevelNow.load();
                                if (result.flyRollSpeed && std::isfinite(wristShown.flyRollSpeed))
                                    flyRollSpeedNow = static_cast<uint32_t>(std::clamp(
                                        std::lround(wristShown.flyRollSpeed), static_cast<long>(extras385::fly::rollSpeedLow),
                                        static_cast<long>(extras385::fly::rollSpeedHigh)));
                                if (result.symbiote)
                                    symbioteOn = !symbioteOn.load();
                                if (result.promptHandling)
                                    promptHandlingNow = promptHandlingNow.load() ? 0u : 1u;
                                if (result.padMode)
                                    padModePressed = true;
                                if (result.playOnMonitor)
                                    monitorPressed = true;
                                if (result.trainMove) { // Version 0.0.2 TRAIN MOVE: the next move's path
                                    trainMovePicked = (trainMovePicked + 1) % training_guide::moves;
                                    ++trainPicks;
                                    char line[96];
                                    sprintf_s(line, "TRAIN MOVE %s - follow the path in the air",
                                              training_guide::moveName(trainMovePicked));
                                    message(3, 0, line);
                                }
                                if (result.gestureMoves) { // Update 38.5: OFF -> TRAINING -> ON
                                    gestureMovesNow = (gestureMovesNow.load() + 1) % gestures385::modes;
                                    char line[64];
                                    sprintf_s(line, "GESTURE MOVES %s", gestures385::modeName(gestureMovesNow.load()));
                                    message(3, 0, line);
                                }
                                if (!remapFault) { // Update 38.5 GESTURE REMAP's rows (EXTRAS)
                                    try {
                                        if (result.remapMove)
                                            gestureRemap.nextMove();
                                        if (result.remapRecord)
                                            gestureRemap.record();
                                        if (result.remapKeep)
                                            gestureRemap.keepAnyway();
                                        if (result.remapAgain)
                                            gestureRemap.tryAgain();
                                        if (result.remapCancel)
                                            gestureRemap.cancel();
                                        if (result.remapResetMove)
                                            gestureRemap.resetMove();
                                        if (result.remapResetAll)
                                            gestureRemap.resetAll();
                                        if (result.remapProfile)
                                            gestureRemap.nextProfile();
                                        remapView = gestureRemap.view();
                                    } catch (...) {
                                        remapFault = true;
                                        remapView = gesture_session::View{};
                                    }
                                }
                                if (result.worldLog) // Update 37: WORLD LOG on/off
                                    world_trace::setOn(!world_trace::isOn());
                                if (result.mirrorRecentre) { // Update 37: RECENTRE MIRROR (shows it too)
                                    mirrorOn = true;
                                    mirrorRecentreAsked = true;
                                    mirrorAutoOffAtMs = 0;
                                }
                                if (result.mirrorAnchor) // Update 37.2: MIRROR ANCHOR, PLAYSPACE <-> WORLD (placed again)
                                {
                                    mirrorAnchor = mirrorAnchor.load() == vr_mirror::world ? vr_mirror::playspace
                                                                                         : vr_mirror::world;
                                    mirrorRecentreAsked = true; // placed again in front of you, in the new frame
                                }
                                if (result.npcFade) // Update 37.2: NPC NEAR FADE, the next mode (live)
                                    npc_fade::set(npc_fade::next(npc_fade::now() & 0xff));
                                // Update 38: a COMBAT row, its next value (live; the session only,
                                // live.txt keeps a value).
                                if (result.combat38 >= 0 && result.combat38 < static_cast<int>(combat38::options)) {
                                    const auto o = static_cast<uint32_t>(result.combat38);
                                    combat38Values[o] = (c38(o) + 1) % combat38::optionInfo[o].count;
                                    heroOnly(o); // Version 0.0.6: one hero toggle at a time
                                    // Version 0.0.62: LENS FLARES is for the next start: the launcher
                                    // reads it back (SpidyHudSettings 3 bit 29, 10) into live.txt.
                                    if (o == combat38::lensFlares)
                                        wristChanged |= 1u << 29;
                                    // 0.0.7 MIRROR PERSIST FIX: MIRROR VIEW is for the next start too (it
                                    // only runs when VR starts): it was never read back, so live.txt kept
                                    // OFF and the restart the wrist asks for turned it off again
                                    // (SpidyHudSettings 3 bit 21, 10 bit 1). Bit 21 is MUSIC PREVIOUS's,
                                    // a press the pages never mark changed (as 29 is MUSIC SEEK's).
                                    if (o == combat38::mirrorView)
                                        wristChanged |= 1u << 21;
                                }
                                if (result.heroRoster && !rosterFault) { // Build 8: the roster page, in front of the head
                                    if (validTrackedPose(frame.head) && rosterPage.open(frame.head, rosterClock))
                                        consoleLine("ROSTER: opened");
                                }
                                if (result.heroSpider) // Update 39.0.1: PLAY AS SPIDER-MAN, every hero toggle off (live)
                                    for (const auto o : combat38::heroToggles)
                                        combat38Values[o] = 0;
                                if (result.colorFx) { // Update 37.2: COLOR FX on/off
                                    colorFxOn = !colorFxOn.load();
                                    colorDirty = true;
                                }
                                if (result.colorFilter) { // Update 37.2: FILTER, the next preset (turns COLOR FX on)
                                    colorFilter = (colorFilter.load() + 1) % eye_color::filters;
                                    colorFxOn = true;
                                    colorDirty = true;
                                }
                                if (result.colorSlider) { // Update 37.2: a COLOR slider moved (live)
                                    const auto whole = [](float f, int lo, int hi) {
                                        return std::clamp(static_cast<int>(std::lround(std::isfinite(f) ? f : 0.f)), lo, hi);
                                    };
                                    colorStrength = static_cast<uint32_t>(whole(wristShown.colorStrength, 0, 100));
                                    colorExposure = whole(wristShown.exposure * 10, -20, 20);
                                    colorContrast = static_cast<uint32_t>(whole(wristShown.contrast, 50, 150));
                                    colorSaturation = static_cast<uint32_t>(whole(wristShown.saturation, 0, 200));
                                    colorVibrance = static_cast<uint32_t>(whole(wristShown.vibrance, 0, 100));
                                    colorWarmth = whole(wristShown.warmth, -100, 100);
                                    colorTint = whole(wristShown.tint, -100, 100);
                                    colorVignette = static_cast<uint32_t>(whole(wristShown.vignette, 0, 100));
                                    colorCartoon = static_cast<uint32_t>(whole(wristShown.colorCartoon, 0, 100));
                                    colorDirty = true;
                                }
                                if (result.colorOutlines) { // Update 38.5: 90s ANIMATED's OUTLINES on/off
                                    colorOutlines = !colorOutlines.load();
                                    colorDirty = true;
                                }
                                if (result.colorBands) { // Update 38.5: 90s ANIMATED's BANDS 3 -> 4 -> 5
                                    colorBands = colorBands.load() >= eye_color::bandLimits.hi ? eye_color::bandLimits.lo
                                                                                               : colorBands.load() + 1;
                                    colorDirty = true;
                                }
                                if (result.foveated) { // Update 37.2: FOVEATED, the next level (EXPERIMENTAL)
                                    foveaLevelWant = (foveaLevelWant.load() + 1) % foveation::levels;
                                    foveaDirty = true;
                                }
                                if (result.foveaInner && std::isfinite(wristShown.foveaInner)) {
                                    foveaInnerWant = std::clamp(static_cast<uint32_t>(std::lround(std::max(0.f, wristShown.foveaInner))),
                                                                foveation::innerMin, foveation::innerMax);
                                    foveaDirty = true;
                                }
                                if (result.foveaDebug) {
                                    foveaDebugWant = !foveaDebugWant.load();
                                    foveaDirty = true;
                                }
                                if (result.foveaStats) // Update 37.2: FOVEATED FPS, a press resets every level's
                                    foveaResetAsked = true;
                                if (result.slotTest) // Update 37.2: SLOT TEST, the next input 1.5 s later
                                    testAsked = 1;
                                if (result.webBallTest) { // Update 37.2: EYE BLOOM OFF <-> COPY, one web ball 1.5 s later
                                    const uint32_t bloom = SpidyEyeBloomNow() & 0xff;
                                    SpidyEyeBloom(reinterpret_cast<void*>(
                                        uintptr_t{bloom == eye_bloom::off ? eye_bloom::copy : eye_bloom::off}));
                                    testAsked = 2;
                                }
                                if (result.sharedLod) // Update 36: SHARED LOD, the next mode (it reverts on its own)
                                    SpidySharedLod(reinterpret_cast<void*>(uintptr_t{shared_lod::nextMode}));
                                if (result.eyeLod) // Update 28: EYE LOD on/off (refused once failed)
                                    SpidyEyeLod(reinterpret_cast<void*>(
                                        static_cast<uintptr_t>(SpidyEyeLodNow() == 1 ? 0 : 1)));
                                if (result.shadowCamera) // Update 31: SHADOW CAMERA, next mode
                                    SpidyShadowCamera(reinterpret_cast<void*>(static_cast<uintptr_t>(
                                        shadow_camera::next(SpidyShadowCameraNow() & 0xff))));
                                if (result.recalibrate)
                                    values.calibrate = true; // the T-pose calibration below, as the tab's
                                if (result.pressed)
                                    runtime.haptic(static_cast<int>(pointHand), .3f);
                                else if (result.changed)
                                    runtime.haptic(static_cast<int>(pointHand), .06f);
                                else if (result.hovered)
                                    runtime.haptic(static_cast<int>(pointHand), .1f);
                                // The values due, each as live.txt's key would set it.
                                const auto apply = [](wrist_hud::Control c, float value) {
                                    using wrist_hud::Control;
                                    value = wrist_hud::clampTo(c, value);
                                    const auto whole = static_cast<uintptr_t>(std::lround(value));
                                    switch (c) {
                                    case Control::thickness:
                                        webRadiusTenthsMm = static_cast<uint32_t>(std::clamp(std::lround(value * 50), 20l, 200l));
                                        return std::pair{true, static_cast<float>(webRadiusTenthsMm.load()) * 2 / 100.f};
                                    case Control::speed:
                                        webSpeedMs = static_cast<uint32_t>(whole);
                                        return std::pair{true, static_cast<float>(webSpeedMs.load())};
                                    case Control::aa:
                                        return std::pair{SpidyEyeAA(reinterpret_cast<void*>(whole)) == 0,
                                                         static_cast<float>(native_gpu::eyeAA())};
                                    case Control::history:
                                        return std::pair{SpidyEyeHistory(reinterpret_cast<void*>(whole)) == 0,
                                                         static_cast<float>(native_gpu::eyeHistory())};
                                    case Control::reach:
                                        return std::pair{SpidyImpostorReach(reinterpret_cast<void*>(whole)) == 0,
                                                         static_cast<float>(SpidyImpostorReachNow())};
                                    case Control::monitor:
                                        return std::pair{SpidyMonitorRate(reinterpret_cast<void*>(whole)) == 0,
                                                         static_cast<float>(native_gpu::monitorRateNow())};
                                    case Control::reticles:
                                        reticlesShown = whole ? 1u : 0u;
                                        return std::pair{true, static_cast<float>(reticlesShown.load())};
                                    // Update 24: EYE TEST (live.txt eye_debug); only the copy to
                                    // the headset changes, never the game's rendering (Update 26: OFF
                                    // or SAME PICTURE again; HEAD SHADOWS is gone).
                                    case Control::eyeTest:
                                        return std::pair{native_gpu::setEyeDebug(whole ? 1u : 0u),
                                                         static_cast<float>(native_gpu::eyeDebug())};
                                    case Control::sharpen:
                                        return std::pair{SpidyEyeSharpen(reinterpret_cast<void*>(whole)) == 0,
                                                         static_cast<float>(native_gpu::eyeSharpen())};
                                    // Update 17: the hologram itself (live.txt hud_*), applied
                                    // when let go (Applier: the panel would move under it).
                                    case Control::hudHeight:
                                        wristHudHeightCm = static_cast<uint32_t>(whole);
                                        return std::pair{true, static_cast<float>(wristHudHeightCm.load())};
                                    case Control::hudScale:
                                        wristHudScalePercent = static_cast<uint32_t>(std::clamp(std::lround(value * 100), 50l, 200l));
                                        return std::pair{true, static_cast<float>(wristHudScalePercent.load()) / 100.f};
                                    case Control::hudText:
                                        wristHudTextPercent = static_cast<uint32_t>(whole);
                                        return std::pair{true, static_cast<float>(wristHudTextPercent.load())};
                                    case Control::hudTheme:
                                        wristHudTheme = static_cast<uint32_t>(whole);
                                        return std::pair{true, static_cast<float>(wristHudTheme.load())};
                                    case Control::hudTextColor:
                                        wristHudTextColor = static_cast<uint32_t>(whole);
                                        return std::pair{true, static_cast<float>(wristHudTextColor.load())};
                                    // Update 18: the webs' look and the style's shade (as
                                    // live.txt web_style and web_color would set them); a
                                    // shade the style has not is refused, put back, noted.
                                    case Control::webStyle:
                                        return std::pair{SpidyWebStyle(reinterpret_cast<void*>(whole)) == 0,
                                                         static_cast<float>(webStyleNow.load())};
                                    case Control::webColor: {
                                        const uint32_t style = std::min<uint32_t>(webStyleNow.load(), web_style::count - 1);
                                        const bool ok = SpidyWebColor(reinterpret_cast<void*>(
                                                            (static_cast<uintptr_t>(style) + 1) << 4 | (whole & 15))) == 0;
                                        return std::pair{ok, static_cast<float>(webShades[style].load())};
                                    }
                                    // Update 19: the music's own volume and the visualizer
                                    // (live.txt music_volume, music_visualizer): live.
                                    case Control::musicVolume:
                                        musicVolumeNow = static_cast<uint32_t>(whole);
                                        return std::pair{true, static_cast<float>(musicVolumeNow.load())};
                                    case Control::musicVisualizer:
                                        musicVisualizerNow = static_cast<uint32_t>(whole);
                                        return std::pair{true, static_cast<float>(musicVisualizerNow.load())};
                                    case Control::supersample:
                                        // Restart required: only kept for the next launch.
                                        nextSupersample = static_cast<uint32_t>(whole);
                                        return std::pair{true, value};
                                    default:
                                        return std::pair{false, 0.f};
                                    }
                                };
                                const uint32_t due = wristApply.due(wristClock, wristMenu.held());
                                for (unsigned c = 0; c < wrist_hud::maskedControls; ++c) {
                                    if (!(due >> c & 1))
                                        continue;
                                    const auto control = static_cast<wrist_hud::Control>(c);
                                    const auto [ok, actual] = apply(control, wristShown[control]);
                                    if (wristApply.result(control, ok, actual, wristClock))
                                        wristChanged |= 1u << c;
                                    else if (!wrist_hud::restartRequired(control))
                                        apply(control, wristApply.revert(control));
                                }
                                wristNote = wristApply.note(wristClock);
                                if (wristNote.empty() && mirrorTooLate)
                                    wristNote = "RESTART THE GAME TO RUN MIRROR VIEW TEST";
                                if (wristNote.empty() && modelMissingMask.load())
                                    wristNote = modelMissingNote(modelMissingMask.load());
                                // Test build 10 MIRROR VIEW: it turned itself OFF - the wrist says why (20 s).
                                if (const uint64_t at = mirrorAutoOffAtMs.load();
                                    wristNote.empty() && at && GetTickCount64() - at < mirror_guard::noteSeconds * 1000) {
                                    char why[96];
                                    if (const uint32_t us = mirrorAutoOffMicros.load())
                                        std::snprintf(why, sizeof why, "MIRROR OFF: TOO SLOW (%.1f MS A FRAME), PRESS MIRROR TO RETRY",
                                                      static_cast<double>(us) / 1000.0);
                                    else
                                        std::snprintf(why, sizeof why, "MIRROR OFF: IT FAILED, PRESS MIRROR TO RETRY");
                                    wristNote = why;
                                }
                                if (wristNote.empty() && mirrorOffWhy && wristClock < mirrorOffNoteUntil &&
                                    mirrorOffWhy < mirror_view::whys) {
                                    wristNote = std::string("MIRROR TEST OFF: ") + mirror_view::whyNames[mirrorOffWhy];
                                    for (auto& ch : wristNote)
                                        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                                }
                                if (wristMenu.claims()) {
                                    hudTriggers |= 1u << pointHand;
                                }
                            } catch (...) {
                                wristMenuFault = true;
                                wristPointer = {};
                                message(3, 0, "Wrist hologram settings failed and are off (save hud = 0, then "
                                              "hud = 1, in live.txt to try them again); everything else carries on");
                            }
                        }
                        for (unsigned i = 0; i < 2; ++i) {
                            if (hudTriggers & (1u << i))
                                controller.hands[i].trigger = 0;
                            // Update 38.1: a grip pressed in the holo zone is the hologram's too.
                            if (hudZone & (1u << i))
                                controller.hands[i].squeeze = 0;
                        }
                        // Update 17: the stick the panel scrolls with is not the game's
                        // (no turn or move) while the pointer is on it.
                        if (hudStick) {
                            frame.hands[pointHand].stickX = frame.hands[pointHand].stickY = 0;
                            controller.hands[pointHand].stickX = controller.hands[pointHand].stickY = 0;
                        }
                    }
                    // A trigger pulled with its hand at the temple is the game's
                    // scan, and no web ball or reel meanwhile.
                    {
                        float triggers[2]{frame.hands[0].trigger, frame.hands[1].trigger};
                        for (unsigned i = 0; i < 2; ++i)
                            if (hudTriggers & (1u << i))
                                triggers[i] = 0;
                        templePress.update(frame.seconds, gesturesPlay, frame.head, grips, gripsTracked,
                                           triggers);
                        for (unsigned i = 0; i < 2; ++i)
                            if (templePress.hands() & (1u << i))
                                controller.hands[i].trigger = 0;
                        if (templePress.scans() != scansBefore) {
                            scansBefore = templePress.scans();
                            runtime.haptic(0, .3f);
                            runtime.haptic(1, .3f);
                            feel(body_haptics::Kind::scan);
                        }
                    }
                    // A grip squeezed at a back pocket draws the phone, and shoots
                    // no web until it is let go. Not with the phone out, nor in the
                    // pad layer, where the grips are the game's bumpers (October
                    // 19: a draw sent the game LB or RB until the phone came out).
                    int phoneDrawnWith = -1;
                    {
                        float squeezes[2]{frame.hands[0].squeeze, frame.hands[1].squeeze};
                        for (unsigned i = 0; i < 2; ++i)
                            if (hudZone & (1u << i))
                                squeezes[i] = 0; // Update 38.1: the holo zone's grip
                        const bool drawPlay = gesturesPlay && !phone.open() && !padLayer.latched();
                        // Version 0.0.7 HOLSTERS (holster.hpp): PUNISHER's hips and back, DAREDEVIL's
                        // BATON HOLSTERS. A grip at a holster draws or holsters there and is the rack's
                        // (no web, zip or climb from it) until let go; a hand on a web or a wall is busy
                        // (never draws). Any other hero: nothing claimed, the grips as in 0.0.62.
                        {
                            hero007::PlayInput hin;
                            hin.dt = hero007HoldAt < 0 ? 0.f : static_cast<float>(std::clamp(frame.seconds - hero007HoldAt, 0.0, .1));
                            hero007HoldAt = frame.seconds;
                            hin.play = drawPlay;
                            hin.hero = heroNow();
                            hin.head = frame.head;
                            hin.crossDraw = combat38::holsterCrossDraw(c38(combat38::holsterHands)); hin.eyeHeight = calibratedEye;
                            hin.batonHolsters = combat38::batonHolstersOn(c38(combat38::batonHolsters));
                            hin.shieldArm = c38(combat38::shieldArm); // Version 0.0.7 CAPTAIN AMERICA
                            for (unsigned i = 0; i < 2; ++i) {
                                hin.hands[i].position = grips[i].position;
                                hin.hands[i].tracked = gripsTracked[i] && validTrackedPose(frame.head);
                                hin.hands[i].squeeze = squeezes[i];
                                hin.hands[i].busy = swingState.webs[i].attached != 0 || u37.climbing[i] ||
                                                    (pocketDraw.hands() & (1u << i)) != 0;
                                hin.hands[i].thrown = !batons[i].inHandNow(); // Version 0.0.7: never 3 batons
                            }
                            hero007::PlayOutput hout;
                            hero007Play.holsterStep(hin, hout);
                            hero007Claimed = hout.claimed;
                            for (unsigned i = 0; i < 2; ++i) {
                                if (hout.haptic[i] > 0)
                                    runtime.haptic(static_cast<int>(i), hout.haptic[i]);
                                if (hero007Claimed & (1u << i)) {
                                    squeezes[i] = 0;
                                    controller.hands[i].squeeze = 0;
                                }
                            }
                            if (hout.sounds & (1u << sfx::holsterTick))
                                sfx::play(sfx::holsterTick);
                        }
                        if (pocketDraw.update(frame.seconds, drawPlay, frame.head, grips, gripsTracked,
                                              squeezes))
                            phoneDrawnWith = pocketDraw.drewWith();
                        for (unsigned i = 0; i < 2; ++i)
                            if (pocketDraw.hands() & (1u << i))
                                controller.hands[i].squeeze = 0;
                    }
                    // The buttons the game names in a prompt, pressed with the
                    // controls of those names (game_pad::NamedPress). A control
                    // it claims is the game's until it is let go, and does
                    // nothing of Spidy's meanwhile: no web ball or reel from that
                    // hand's web-ball button, no jump or flip from A, no turn
                    // from the right stick pushed for the game's D-pad.
                    {
                        const auto asked = game_prompts::demand();
                        game_pad::NamedPress::Input named;
                        named.buttons = frame.buttons;
                        // The temple's scan has a trigger, the pocket's draw a grip.
                        const uint32_t gestures =
                            values.triggerWebs ? pocketDraw.hands() : templePress.hands() | hudTriggers;
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& hand = frame.hands[i];
                            named.balls[i] = values.triggerWebs ? hand.squeeze : hand.trigger;
                            named.ballsTaken[i] = (gestures & (1u << i)) != 0;
                            named.clicks[i] = hand.stickClick;
                            named.hands[i] = hand.grip.position - frame.head.position;
                            named.tracked[i] = gripsTracked[i] && validTrackedPose(frame.head);
                            named.busy[i] = swingState.webs[i].attached != 0;
                            u37.ball[i] = named.balls[i];
                            u37.other[i] = values.triggerWebs ? hand.trigger : hand.squeeze;
                            // Update 38.1: the holo zone's presses fire no ball, gadget or Sonic Burst.
                            if (hudZone & (1u << i))
                                u37.ball[i] = u37.other[i] = 0;
                        }
                        named.stickX = frame.hands[1].stickX;
                        named.stickY = frame.hands[1].stickY;
                        named.airborne = game_swing::airborne(swingState, now.QuadPart, frequency.QuadPart);
                        const bool namedPlay =
                            gesturesPlay && !padLayer.latched() && !phone.open() && !gadgetWheel.open();
                        namedPad = namedPress.update(frame.seconds, namedPlay, named, asked);
                        const uint32_t claimed = namedPress.claimed();
                        if (claimed & game_pad::NamedPress::controlA) {
                            controller.jump = false;
                            controller.buttons &= ~static_cast<uint32_t>(buttonA);
                        }
                        for (unsigned i = 0; i < 2; ++i) {
                            auto& hand = controller.hands[i];
                            if (claimed & game_pad::NamedPress::ball(i))
                                (values.triggerWebs ? hand.squeeze : hand.trigger) = 0;
                            u37.taken[i] = named.ballsTaken[i] || (claimed & game_pad::NamedPress::ball(i)) != 0;
                        }
                        if (claimed & (game_pad::NamedPress::stickLeft | game_pad::NamedPress::stickRight))
                            controller.hands[1].stickX = 0;
                        // While the game names its X a thrown fist is that X, and
                        // the swing lands no blow of its own (sendSwingSettings).
                        namedFists = namedPlay && (asked.buttons & game_pad::x) != 0;
                    }
                    // The player's up (its actor's second row): a wall's normal
                    // while the game holds it on one. Spidy's own flight is on none.
                    const Vec3 heroUp{body[4], body[5], body[6]};
                    // The wall that holds the player, as the latest swing sample
                    // has it (no older than a tenth of a second): the swing's own
                    // wall, its floor on the wall beside the body's centre, or
                    // the game's own crawl, whose actor has the feet on the
                    // surface itself.
                    //
                    // Both of them, since October 14. The crawl was held back
                    // while how it takes the stick went unmeasured, and that
                    // left the one state whose interacts work on a wall - the
                    // only one the game calls supported there - looking nothing
                    // like the wall walking the player asked to keep, so a vent
                    // cost him the view. The rig's own crawl path was built and
                    // tested the whole time; only this passed it nothing.
                    const Vec3 feet{body[12], body[13], body[14]};
                    SurfaceHold hold;
                    const bool swingFresh =
                        swingState.status && swingState.qpc &&
                        static_cast<int64_t>(swingState.qpc) > now.QuadPart - frequency.QuadPart / 10;
                    if (swingFresh && swingState.wall == static_cast<uint32_t>(game_swing::Surface::wall)) {
                        // The wall's face under the feet, in the wall's own frame
                        // (its distance is the body's centre's, a metre over the
                        // feet): the same floor the game's crawl stands the hero
                        // on, so going from the one hold to the other moves no one.
                        hold.normal = swingState.wallNormal;
                        hold.anchor = feet - hold.normal * (swingState.wallDistance - hold.normal.y);
                        hold.speed = length(swingState.velocity);
                    } else if (swingFresh && swingState.wall == static_cast<uint32_t>(game_swing::Surface::game)) {
                        // The crawl's feet are on the surface, so they are the
                        // floor under the player, and its normal comes from a
                        // ray where the swing has one. The actor's own up is
                        // that surface while it rocks on it (up to 14 degrees
                        // off over a few frames, reports/wall-crawl.json) and
                        // serves where the ray found nothing. In the game's own
                        // wall run the feet are off the wall by the distance
                        // the swing measured: the floor is the wall under them.
                        hold.normal = finite(swingState.wallNormal) && length(swingState.wallNormal) > .5f
                                          ? normalized(swingState.wallNormal)
                                          : normalized(heroUp);
                        hold.anchor = feet - hold.normal * (std::isfinite(swingState.wallDistance)
                                                                ? std::clamp(swingState.wallDistance, 0.f, 1.5f)
                                                                : 0.f);
                        hold.speed = length(swingState.velocity);
                        hold.crawl = true;
                        ++crawlFrames;
                    } else if (swingFresh && swingState.wall == static_cast<uint32_t>(game_swing::Surface::floor)) {
                        // The game's crawl on a floor: a vent, a duct. The
                        // floor is under the feet and level, whatever the
                        // crawling actor rocks to: the view stands as it
                        // stands, the body plants its feet on it (the game's
                        // crawl laid the legs out in front of the player in
                        // the first mission's duct, October 14 19:57), and
                        // the head stays under its roof (below).
                        hold.normal = {0, 1, 0};
                        hold.anchor = feet;
                        hold.speed = length(swingState.velocity);
                        hold.crawl = true;
                        ++floorCrawlFrames;
                    }
                    const bool crawlsFloor = hold.crawl && hold.normal.y > .99f;
                    floorCrawlSeconds =
                        crawlsFloor ? floorCrawlSeconds + (std::isfinite(frame.seconds)
                                                               ? std::clamp(frame.seconds, 0.f, .1f)
                                                               : 0.f)
                                    : 0.f;
                    // Between the swing's wall and the game's crawl (game_warp:
                    // the game turning the hero onto the wall at rest, or off it
                    // as he walks on) the swing reports neither for a moment.
                    // The view keeps the surface it stood him on while the body
                    // stays at its floor, slower along it than a walk: dropped,
                    // the view turned back toward level and onto the wall again
                    // (48 degrees in a tenth of a second, October 14 16:48). A
                    // jump or a run off the wall leaves it at once.
                    {
                        constexpr float holdGrace = .4f, holdGraceSpeed = .8f;
                        const float seconds =
                            std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                        const Vec3 n = lastHold.normal;
                        const Vec3 v = finite(swingState.velocity) ? swingState.velocity : Vec3{};
                        const bool held = length(hold.normal) > .5f;
                        holdGapSeconds = held ? 0.f : holdGapSeconds + seconds;
                        if (held) {
                            lastHold = hold;
                        } else if (motion.standing && length(n) > .5f && holdGapSeconds < holdGrace &&
                                   std::abs(dot(feet - lastHold.anchor, n)) < 1.5f &&
                                   length(v - n * dot(v, n)) < holdGraceSpeed) {
                            // On the plane it stood on, with the feet: kept
                            // where it was while the game took a player over
                            // a shaft's lip at 1.3 m/s, the view stayed on
                            // the wall a third of a second after he had left
                            // it and then jumped to him (October 14, 86 s).
                            // Only for a player at rest: one on his way off
                            // the surface is on his way off it.
                            hold = lastHold;
                            hold.anchor = feet - n * dot(feet - lastHold.anchor, n);
                            ++holdGraceFrames;
                        } else {
                            lastHold = {};
                        }
                    }
                    perfSections.mark(perf37::menu); // Update 37: where the CPU time goes
                    // A or the left stick in the air flips the player (FlipMotion),
                    // in the air as the latest swing sample has it.
                    // In the game's crawl on a floor the head goes no higher
                    // than a crouch, whatever a ray finds over it: part of the
                    // first mission's duct has a roof no ray hits, and the
                    // head stood out through it at the player's own height.
                    // Update 38 CRAWL CAP: no longer on the crawl flag alone (it held the head
                    // low in fights, run372): AUTO only under a roof under 1.6 m seen on 3+
                    // readings over 0.3 s and not within 1.5 s of the fight's input; WITH FLAG
                    // also on the flag (37.2's duct case); OFF never (combat38::CrawlCap).
                    float over = headroomSeen;
                    if (crawlGuard.update({frame.seconds, headroomSeen, roofFresh, floorCrawlSeconds > 0, combatInputLast},
                                          c38(combat38::crawlCap))) {
                        over = over > 0 ? std::min(over, crawlRoom) : crawlRoom;
                        ++crawlCapFrames;
                    }
                    roofFresh = false;
                    rig.headroom(over);
                    if (headroomSeen > 0 && (headroomLeast == 0 || headroomSeen < headroomLeast))
                        headroomLeast = headroomSeen;
                    // The eyes out of what the world is built of (HeadClearance): from a point in
                    // the body, where no wall is - half a metre off the wall that holds him, or
                    // a metre up from his feet along his up (the world's while Spidy's swing
                    // has him, which the game keeps upright in its air state) - and the
                    // surfaces the head's rays met in the batch the bridge last finished,
                    // facing them and of what the world is built of (a car, a crate or a
                    // passer-by moves, and is no wall).
                    {
                        const Vec3 held = normalized(hold.normal);
                        const Vec3 bodyUp = swingState.owned ? Vec3{0, 1, 0} : normalized(heroUp);
                        const bool holds = finite(held) && length(held) > .5f && finite(hold.anchor);
                        // A thick corner pillar can cover the old half-metre
                        // point. Start climbing camera rays inside the actual
                        // native capsule so its front cannot be ignored.
                        rig.bodyCentre(
                            headCollisionOrigin(feet, bodyUp, hold, values.handClimb && swingState.owned));
                        if (holds)
                            rig.surfaceFound({hold.anchor, held}, true);
                        if (rayResults)
                            for (unsigned i = 0; i < native_rays::capacity && i < rayResults->count; ++i) {
                                const auto& hit = rayResults->hits[i];
                                const Vec3 n = normalized(hit.normal);
                                if (hit.ray.tag >= 2 && hit.ray.tag <= 6 && native_rays::fixedSurface(hit) &&
                                    std::isfinite(hit.fraction) && hit.fraction < 1 && finite(n) &&
                                    dot(n, hit.ray.direction) < -.2f)
                                    rig.surfaceFound({hit.position, n});
                            }
                    }
                    // REALISTIC FLIPS turn with the swing's flight: its
                    // velocity, and while it has the player each web's pull
                    // toward where it holds, by its tension.
                    RealisticFlight flight;
                    if (values.realisticFlips) {
                        const int64_t fresh = now.QuadPart - frequency.QuadPart / 10;
                        flight.valid = swingState.qpc && static_cast<int64_t>(swingState.qpc) > fresh &&
                                       !swingState.error &&
                                       (swingState.status == 1 || swingState.status == 2);
                        flight.velocity = swingState.velocity;
                        float ropes{};
                        unsigned attached{};
                        if (flight.valid && swingState.owned)
                            for (const auto& web : swingState.webs) {
                                const Vec3 toward = web.anchor - feet;
                                if (!web.attached || !finite(web.anchor) || !std::isfinite(web.tension) ||
                                    length(toward) < .1f)
                                    continue;
                                flight.attached = true;
                                ropes += length(toward);
                                ++attached;
                                flight.pull += normalized(toward) * std::clamp(web.tension, .1f, 100.f);
                            }
                        if (attached)
                            flight.ropeLength = ropes / static_cast<float>(attached);
                    }
                    rig.flight(flight);
                    rig.wallTilt(c38(combat38::wallTilt)); // Update 38 WALL TILT
                    rig.handFlips(combat38::handFlipsOn(c38(combat38::handFlips))); // Version 0.0.62 HAND FLIPS
                    { // Update 38.5 FLY ROLL: the right stick's x rolls the view while flying (not turning);
                      // levels itself while hovering or landed. A mod-side view transform, not the game camera.
                        float rollAngle = 0;
                        bool stickRolls = false;
                        try {
                            const bool rolling = flyModeNow.load() != 0 && flyNow.flying && flyNow.moving &&
                                                 !flyNow.descending && gameplay;
                            rollAngle = flyRollState.update(frame.seconds, flyRollNow.load(),
                                                            static_cast<float>(flyRollSpeedNow.load()),
                                                            flyAutoLevelNow.load(), rolling, frame.hands[1].stickX);
                            stickRolls = flyRollState.stickRolls();
                        } catch (...) {
                            flyRollState.reset();
                            flyRollNow = extras385::fly::rollOff;
                            game_prompts::note("Update 38.5: FLY ROLL failed and turned itself off");
                        }
                        rig.flyRoll(std::isfinite(rollAngle) ? rollAngle : 0.f, stickRolls);
                    }
                    motion = rig.update(controller, feet, {camera[8], camera[9], camera[10]}, gameplay,
                                        swingState.owned ? Vec3{0, 1, 0} : heroUp,
                                        game_swing::airborne(swingState, now.QuadPart, frequency.QuadPart), hold,
                                        {camera[4], camera[5], camera[6]});
                    if (motion.headDrop > .02f)
                        ++headDroppedFrames;
                    if (motion.active && motion.onSurface) {
                        surfaceEntries += !wasOnSurface;
                        ++surfaceFrames;
                    }
                    wasOnSurface = motion.active && motion.onSurface;
                    // Without VR play (menus, hint cards, cutscenes, animated
                    // cameras, loading, and the game played on its screen,
                    // VIEW: GAME SCREEN) the game's own camera goes on the
                    // screen.
                    const bool vrPlay = motion.active && !flatScreen;
                    bool screen{};
                    if (viewing)
                        screen = gameScreen.update(vrPlay, GetTickCount64());
                    else
                        gameScreen.reset();
                    // Update 32.1: eyes paused after a graphics setting rebuilt
                    // them in a way the bridge cannot follow (eye_swap.hpp):
                    // the game screen with a notice, never the flat window.
                    if (viewing && gpuStarted && native_gpu::eyesLost())
                        screen = true;
                    wasScreen = screen;
                    // A scene the game plays there (a cutscene, a finisher, a
                    // death) gets its own two eyes: the game renders its
                    // camera once for each of the viewer's. Not a pause menu
                    // or a hint card (the game's frames take no game time),
                    // nor without a player (the title screen, a load), nor a
                    // scene whose eyes get no HUD panel for its subtitles.
                    // Version 0.0.70: nor without the HUD panel's hook at all
                    // (SM2: not hooked), so the game renders no hidden eye
                    // views at each scene start; the flat screen stays.
                    const auto clock = game_time::telemetry();
                    const bool timeRuns =
                        sceneClock.running(clock.installed && clock.gameFrame > 0, GetTickCount64());
                    const bool scene = scenePanel.wanted(
                        sceneStereoAsked(screen && player.hero && bridgeRunning && timeRuns, values.screenDepth > 0,
                                         native_hud::panelHooked()),
                        native_hud::onScreen(sceneHoldMs), GetTickCount64());
                    // On the screen the controllers are the game's Xbox
                    // controller; in VR it gets Start (menu), Back (X) and
                    // the native walking, jumping, dodge and interact
                    // (submitted below).
                    // The pad layer is latched in the input block below, where the
                    // swing's state is known, so this reads the last frame's: the
                    // chord takes chordSeconds to latch, so a frame is nothing.
                    // Nothing reaches the game until the headset has tracked
                    // once: before that the player presses blind, and the title
                    // screen's QUIT GAME is two moves from the top.
                    headSeen = headSeen || validTrackedPose(frame.head);
                    const auto mapping =
                        game_pad::mappingFor(viewing, headSeen, screen, padLayer.latched());
                    const bool inPlay =
                        mapping == game_pad::Mapping::gameplay || mapping == game_pad::Mapping::pad;
                    origins.update(frame.buttons, inPlay);
                    // What the controllers themselves sent, beside what reached the
                    // game: a button missing from both was never pressed. The
                    // phone's presses are the controllers' too.
                    if (inPlay)
                        vrButtonsPlayed |= trackedFrame.buttons;
                    // The prompts are read wherever the game's UI builds one: what
                    // they name is what the controls of those names send in VR
                    // play (NamedPress). Off from October 13 to 20, because with
                    // the reading on CONTINUE at the main menu killed the game
                    // every time - the process simply gone, no crash dump. The
                    // death was the reading's own: the timeline's line for a
                    // prompt call went through sprintf_s into a buffer a long
                    // text overflowed, and sprintf_s ends the process for that
                    // (game_prompts.cpp, keepLine). The wording stays the game's:
                    // a text is never rewritten (setRewriting).
                    game_prompts::setObserving(promptHelp);
                    game_prompts::setReading(promptHelp);
                    game_prompts::setRewriting(false);
                    // A change in the SPIDY VR tab takes effect at once.
                    if (game_menu::take(values)) {
                        ++settingChanges;
                        applySettings();
                        if (auto text = vr_settings::tabOnlyText(values); text != tabOnlySaved) {
                            if (!writeTabOnly(text))
                                message(3, 0, "VR settings: the SPIDY VR tab's own settings were not saved");
                            tabOnlySaved = std::move(text);
                        }
                    }
                    game_menu::publish(values);
                    const uint32_t gate =
                        (player.hero ? 0 : gateNoPlayer) | (bridgeRunning ? 0 : gateBridge) |
                        (committed ? 0 : gateNoCommit) | (followsPlayer ? 0 : gateOtherCamera) |
                        (gameplay && !motion.active ? gateTracking : 0);
                    if (gameplay && !motion.active) {
                        ++trackingGapFrames;
                        trackingGapReasons |= motion.inactive;
                    }
                    if ((gameScreen.entered() && !flatScreen) || ((flatScreen || screen) && frame.recentered))
                        screenPose = screenAhead(frame.head, vr_settings::screenDistance);
                    // An image keeps the tracking-space poses it was rendered
                    // for, valid until the tracking space itself changes.
                    if (!viewing || frame.recentered)
                        history.clear();
                    if (!viewing || frame.recentered || !motion.active || flatScreen)
                        hudFollow.reset();
                    if (!viewing) {
                        lastPresentedMs = lastNewImageMs = firstImageDeadline = 0;
                        presented = false;
                    } else if (vrPlay && !wasGameplay) {
                        // Gameplay counts its own images: the screen before it
                        // may have had none for a while (loading).
                        lastNewImageMs = 0;
                        presented = false;
                        firstImageDeadline = nativeStarted ? GetTickCount64() + 3000 : 0;
                    }
                    wasGameplay = vrPlay;
                    const bool controlsVisible = recentPresentation(lastNewImageMs, GetTickCount64());
                    // The T-pose calibration: at the first gameplay of a session
                    // without one, or when the SPIDY VR tab asks for it, once VR
                    // has shown play for a moment and no web flies the player. It
                    // goes on only while VR shows play (a pause or the flat
                    // screen holds it), and B skips it.
                    using CalibrationPhase = body_calibration::Phase;
                    const bool immersive = viewing && motion.active && !flatScreen && controlsVisible;
                    const uint64_t nowMs = GetTickCount64();
                    if (!immersive)
                        immersiveSince = 0;
                    else if (!immersiveSince)
                        immersiveSince = nowMs;
                    const bool skipPressed = (frame.buttons & buttonB) && !skipHeld;
                    skipHeld = (frame.buttons & buttonB) != 0;
                    if (calibration.phase() == CalibrationPhase::idle && (calibrationWanted || values.calibrate) &&
                        immersive && nowMs - immersiveSince >= 1500 && !swingState.owned) {
                        calibration.start();
                        calibrationPanel = body_calibration::panelPose(frame.head);
                        calibrationInput = true;
                        values.calibrate = false; // the tab shows NO again
                        runtime.haptic(0, .3f);
                        runtime.haptic(1, .3f);
                        message(3, 0, "Body calibration: stand in a T-pose and hold both triggers (B skips)");
                    } else if (calibration.phase() == CalibrationPhase::done) {
                        if (nowMs - calibrationShownAt >= 3500)
                            calibration.stop();
                    } else if (calibration.phase() != CalibrationPhase::idle && immersive && skipPressed) {
                        calibration.stop();
                        ++calibrationSkips;
                        // Nor at the next sessions' first gameplay (the launcher keeps it).
                        calibrationWanted = calibrationPrompt = false;
                        runtime.haptic(1, .25f);
                        message(3, 0, "Body calibration skipped - Settings > SPIDY VR > CALIBRATE BODY does it later");
                    } else if (calibration.phase() != CalibrationPhase::idle && immersive) {
                        body_calibration::Sample sample;
                        sample.headTracked = validTrackedPose(frame.head);
                        sample.head = frame.head;
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& hand = frame.hands[i];
                            sample.handTracked[i] = hand.valid && validTrackedPose(hand.grip);
                            sample.grips[i] = hand.grip;
                            sample.triggers[i] = hand.trigger;
                        }
                        sample.seconds = frame.seconds;
                        // The hero's rig once the body has turned it, else Spider-Man's measured one.
                        body_calibration::Proportions sizes;
                        native_body::proportions(sizes);
                        const auto before = calibration.phase();
                        if (calibration.update(sample, sizes)) {
                            lastCalibration = calibration.result();
                            calibratedEye = std::clamp(lastCalibration.eyeHeight,
                                                       body_calibration::minEyeHeightMm / 1000.f,
                                                       body_calibration::maxEyeHeightMm / 1000.f);
                            calibratedArm = std::clamp(lastCalibration.armLength,
                                                       body_calibration::minArmLengthMm / 1000.f,
                                                       body_calibration::maxArmLengthMm / 1000.f);
                            ++calibrations;
                            calibrationWanted = false;
                            calibrationShownAt = nowMs;
                            runtime.haptic(0, .7f);
                            runtime.haptic(1, .7f);
                            feel(body_haptics::Kind::calibrated);
                            char text[160];
                            sprintf_s(text, "Body calibrated: eye height %.2f m, arm length %.2f m (left %.2f, right %.2f)",
                                      calibratedEye, calibratedArm, lastCalibration.reach[0], lastCalibration.reach[1]);
                            message(3, 0, text);
                        } else if (before == CalibrationPhase::waiting &&
                                   calibration.phase() == CalibrationPhase::holding) {
                            // The pose counts: a light tick in both hands.
                            runtime.haptic(0, .15f);
                            runtime.haptic(1, .15f);
                        }
                    }
                    drainConsoleLines(); // build 5: the queued console lines, one each 250 ms
                    {
                        static int modelsWereReady = -1; // -1: not looked yet (the start's own line said it)
                        const int ready = overlay.modelReady() ? 1 : 0;
                        if (modelsWereReady == 1 && !ready)
                            consoleLine("MODELS: UNTEXTURED from now - " + overlay.modelFault());
                        modelsWereReady = ready;
                    }
                    // Version 0.0.72 TEST BUILD NOTICE: it reads the hands, never takes them from play.
                    if (test_notice::testBuild && !testNoticeFault) {
                        try {
                            test_notice::Input in;
                            // Build 5 fix (the build 4 "no popup"): frame.seconds is the frame's LENGTH
                            // (about 0.011), never a clock, so the notice's 1 s settle never passed.
                            // Its own clock now: the frames' lengths added up.
                            testNoticeClock += std::isfinite(frame.seconds) ? std::clamp(static_cast<double>(frame.seconds), 0.0, .1) : 0.0;
                            in.seconds = testNoticeClock;
                            in.immersive = immersive;
                            in.calibrating = calibration.phase() != CalibrationPhase::idle;
                            in.calibrationDue = (calibrationWanted || values.calibrate) && testNoticeClock < 45; // build 5: never waits on a prompt forever
                            in.head = validTrackedPose(frame.head) ? frame.head : Pose{{NAN, 0, 0}, {}};
                            for (unsigned i = 0; i < 2; ++i) {
                                const auto& hand = frame.hands[i];
                                in.hands[i].tracked = hand.valid && validTrackedPose(hand.grip);
                                in.hands[i].grip = hand.grip;
                                in.hands[i].aim = hand.aim;
                                in.hands[i].trigger = hand.trigger;
                                in.hands[i].a = noticeA[i]; // build 6: A closes it
                            }
                            const auto before = testNotice.state();
                            testNotice.update(in);
                            if (before != testNotice.state() && testNotice.state() == test_notice::State::shown) {
                                runtime.haptic(0, .2f), runtime.haptic(1, .2f);
                                consoleLine("TEST NOTICE: shown (HERO VR 0.0.72 test " SPIDY_TEST_COMMIT ")");
                            }
                            else if (before == test_notice::State::shown && testNotice.state() == test_notice::State::gone &&
                                     testNotice.ended() != test_notice::Ended::timedOut)
                                {
                                runtime.haptic(testNotice.ended() == test_notice::Ended::poked ? 1 : 0, .25f);
                                if (testNotice.ended() == test_notice::Ended::pressedA)
                                    consoleLine("TEST NOTICE: closed with A (A kept from play until let go)");
                            }
                        } catch (...) {
                            testNoticeFault = true; // only the notice goes off
                        }
                    }
                    // The phone, which a back-pocket draw opens in the hand. It is
                    // live while VR shows play and no calibration has the
                    // controllers, which it leaves to a calibration (October 19:
                    // one begun under it could be neither finished nor skipped).
                    {
                        const bool phonePlay = immersive && calibration.phase() == CalibrationPhase::idle;
                        if (phone.update(frame.seconds, phonePlay, frame.head, grips, gripsTracked,
                                         phoneDrawnWith, pocketDraw.hand())) {
                            phoneMusic.load();
                            runtime.haptic(0, .2f);
                            runtime.haptic(1, .2f);
                            feel(body_haptics::Kind::phone, pocketDraw.drewWith());
                            message(3, 0, "Phone out - the stick, A, B and the triggers are the phone's "
                                          "until B puts it away");
                        }
                    }
                    // While it is out its hands land no blow (Settings::noBlows),
                    // nor while the game names its X (NamedPress, above).
                    if (phone.live() != phoneBlows || namedFists != fistsBlows)
                        sendSwingSettings();
                    if (calibrationInput && calibration.phase() == CalibrationPhase::idle) {
                        bool held = (frame.buttons & buttonB) || frame.jump;
                        for (const auto& hand : frame.hands)
                            held = held || hand.trigger > .2f || hand.squeeze > .2f;
                        calibrationInput = held;
                    }
                    if (calibrationInput) {
                        // The calibration has the controllers: no web, web ball or jump.
                        motion.swing.jump = false;
                        motion.nativeKeys &= ~swingJumpKey;
                        for (auto& hand : motion.swing.hands)
                            hand.trigger = hand.grip = 0;
                    }
                    if (padLayer.latched()) {
                        // The pad layer has them too: the grips are the game's bumpers
                        // there and the triggers its triggers, so neither shoots a web.
                        // A is the game's own A straight from the controller, so the
                        // swing's leap goes with them.
                        motion.swing.jump = false;
                        motion.nativeKeys &= ~swingJumpKey;
                        for (auto& hand : motion.swing.hands)
                            hand.trigger = hand.grip = 0;
                    }
                    perfSections.mark(perf37::motion); // Update 37: where the CPU time goes
                    // Slow motion: the left thumbstick's click, in VR play outside the calibration.
                    const bool slowAllowed = immersive && calibration.phase() == CalibrationPhase::idle;
                    // A click the game names (its L3, its R3) is not slow motion.
                    const uint32_t clicksNamed = namedPress.claimed();
                    const bool slowLeft =
                        frame.hands[0].stickClick && !(clicksNamed & game_pad::NamedPress::leftClick);
                    const bool slowRight =
                        frame.hands[1].stickClick && !(clicksNamed & game_pad::NamedPress::rightClick);
                    const bool slowPressed =
                        slowPress.update(frame.seconds, slowAllowed, slowLeft, slowRight);
                    stepSlowMotion(frame.seconds, slowPressed, slowAllowed);
                    if (SM2_PORT_GAMEPLAY && motion.active && !raysStarted && !raysFailed) {
                        native_rays::Config rays;
                        rays.pid = config.pid;
                        rays.base = config.base;
                        rays.durationMs = moduleDuration;
                        // Fail safe: a refusal (another game build's bytes, a
                        // hook MinHook will not take) leaves rays - and the
                        // swing, which casts through them - off; VR goes on.
                        if (const auto code = startRays ? startRays(&rays) : 1u) {
                            raysFailed = true;
                            const auto text = "Controller world rays and swinging are off: " +
                                              gameplayRefusal(code) + " (" + std::to_string(code) +
                                              "); VR continues";
                            message(3, code, text.c_str());
                        } else {
                            raysStarted = true;
                        }
                    }
                    if (raysStarted) {
                        auto input = motion.swing;
                        input.focused = motion.active && !flatScreen;
                        auto aim = controllerAimRays(input, ++serial);
                        enemiesHereCount = std::min<uint32_t>(world_trace::enemies(enemiesHere, &enemiesHereIds),
                                                              world_trace::enemySlots);
                        // The head's rays, in the slots the hands leave free
                        // (HeadClearance): from the body to past the eyes, and
                        // from the eyes ahead, to each side and up. Until
                        // October 18 these five read the room around the head
                        // for the report alone (Enclosure).
                        if (motion.active && validTrackedPose(motion.headPose)) {
                            const Quat turn = motion.headPose.orientation;
                            const Vec3 ahead = turn.rotate({0, 0, -1}), aside = turn.rotate({1, 0, 0}),
                                       overhead = turn.rotate({0, 1, 0});
                            const auto head = rig.clearanceRays(ahead, aside, overhead);
                            for (unsigned i = 0; i < head.size() && aim.count < native_rays::capacity; ++i)
                                if (head[i].distance >= .01f && finite(head[i].origin) &&
                                    std::abs(length(head[i].direction) - 1) < .001f)
                                    aim.rays[aim.count++] = {head[i].origin,
                                                             std::min(head[i].distance, 150.f),
                                                             head[i].direction, 2u + i};
                            // And the room over his feet, along the surface he
                            // stands on: how far the head can go before it is
                            // through the roof of a vent or a duct. From the
                            // FEET, where the other five are from the head - a
                            // head already through that roof would cast from
                            // the far side of it and find nothing.
                            //
                            // Along the up the head is actually placed on: the
                            // surface's normal while the view stands on it, the
                            // world's up otherwise, because that is the axis the
                            // rig caps. The actor's own up is neither while
                            // Spidy holds him on a wall - the game has him
                            // upright in its air state there - and casting along
                            // it found the shaft's ceiling 0.16 m over his feet
                            // and brought his head down to a crouch for 22
                            // frames (reports/game-vr-20261014-124656.json).
                            const Vec3 up = motion.standing && finite(hold.normal) && length(hold.normal) > .5f
                                                ? normalized(hold.normal)
                                                : Vec3{0, 1, 0};
                            // Update 38: every 3rd frame with enemies near, a sight ray from the
                            // head to one of them (to his chest; within 45 m, the aim assist's
                            // reach) takes the roof ray's slot; the roof is read the other two.
                            // Version 0.0.62 BOLT WALLS: a shot that needs its wall takes that slot this
                            // frame (the roof and the sights keep their last reading for it): the chest
                            // beam first, then the flying shots in turn - one not already asked within
                            // 4 frames, its ray from where it is along its flight for what it has left.
                            bool sighted{};
                            ++wallFrame;
                            if (aim.count < native_rays::capacity) {
                                const Pose shotToWorld = compose(motion.headPose, inverse(frame.head));
                                if (beamWait.waiting && !beamWait.asked && finite(beamWait.fromWorld) &&
                                    std::abs(length(beamWait.dirWorld) - 1) < .001f) {
                                    aim.rays[aim.count++] = {beamWait.fromWorld, repulsor::beamRange, beamWait.dirWorld,
                                                             bolt_walls::beamTag(beamWait.serial)};
                                    beamWait.asked = true;
                                    sighted = true;
                                    ++wallCasts;
                                }
                                for (unsigned k = 0; k < our_ball::Pool::balls && !sighted && finite(shotToWorld.position); ++k) {
                                    const unsigned slot = (shotCastNext + k) % our_ball::Pool::balls;
                                    const auto& b = oursBalls.ball(static_cast<int>(slot));
                                    if (!b.alive || !b.serial)
                                        continue;
                                    const Vec3 at = compose(shotToWorld, Pose{b.at, {}}).position;
                                    const Vec3 way = shotToWorld.orientation.rotate(b.direction);
                                    if (!finite(at) || !finite(way) || length(way) < .5f)
                                        continue;
                                    if (shotWallSerial[slot] == b.serial && !bolt_walls::stale(shotWallsWorld[slot], at, way))
                                        continue;
                                    if (shotAskedSerial[slot] == b.serial && wallFrame - shotAskedFrame[slot] < 4)
                                        continue;
                                    const float left = std::clamp((b.life - b.age) * our_ball::speed + 1.f, 1.f, 150.f);
                                    aim.rays[aim.count++] = {at, left, normalized(way), bolt_walls::ballTag(static_cast<int>(slot), b.serial)};
                                    shotAskedSerial[slot] = b.serial;
                                    shotAskedFrame[slot] = wallFrame;
                                    shotCastNext = slot + 1;
                                    sighted = true;
                                    ++wallCasts;
                                }
                            }
                            // Version 0.0.72 PUNISHER VAN SUMMON: the probe's one ray (the roof's slot, as the beam's),
                            // from his feet along his look; asked again after 12 frames with no answer.
                            if (vanWanted && !vanProbe.active && finite(feet)) {
                                vanProbe.start(feet, ahead);
                                vanWanted = vanAsking = false;
                            }
                            if (vanProbe.active && vanAsking && ++vanAskFrames > 12)
                                vanAsking = false;
                            if (vanProbe.active && !vanAsking && !sighted && aim.count < native_rays::capacity) {
                                van072::Ray vr{};
                                if (vanProbe.next(vr) && finite(vr.from) && std::abs(length(vr.dir) - 1) < .001f) {
                                    vanAskTag = vanTag0 + (vanAsks++ % vanTags);
                                    aim.rays[aim.count++] = {vr.from, std::min(vr.distance, 150.f), vr.dir, vanAskTag};
                                    vanAsking = true;
                                    vanAskFrames = 0;
                                    sighted = true;
                                } else
                                    vanProbe = {}; // a ray it cannot ask: no spot (never a guess)
                                if (!vanProbe.active) {
                                    vanSpot = vanProbe.result;
                                    vanSpotReady = true;
                                }
                            }
                            if (!sighted && ++sightFrame % 3 == 0 && enemiesHereCount && aim.count < native_rays::capacity)
                                for (uint32_t tries = 0; tries < enemiesHereCount && !sighted; ++tries) {
                                    const uint32_t k = sightNext++ % enemiesHereCount;
                                    const Vec3 from = motion.headPose.position;
                                    const Vec3 chest = motion.anchor + enemiesHere[k] + Vec3{0, .9f, 0};
                                    const float metres = length(chest - from);
                                    if (!enemiesHereIds[k] || !finite(chest) || !std::isfinite(metres) ||
                                        metres < .5f || metres > 45.f)
                                        continue;
                                    aim.rays[aim.count++] = {from, metres, (chest - from) / metres, 100u + k};
                                    sightAsks[sightAskNext++ % sightAsks.size()] = {aim.serial, enemiesHereIds[k]};
                                    ++sightRays;
                                    sighted = true;
                                }
                            if (!sighted && finite(up) && length(up) > .5f && aim.count < native_rays::capacity)
                                aim.rays[aim.count++] = {feet + up * .1f, headroomReach, up, 7u};
                            aim.leaseMs = 100;
                        }
                        if (const auto code = submitRays(&aim))
                            raysOff(code, "the ray bridge refused a batch");
                        // What the roof's ray found, from the batch the bridge
                        // last finished (the head's go to the rig before its
                        // update, above).
                        if (rayResults) {
                            // Update 38: a new batch's roof reading is a sample for the CRAWL CAP.
                            const bool newBatch = rayResults->serial != roofSerial;
                            roofSerial = rayResults->serial;
                            for (unsigned i = 0; i < native_rays::capacity && i < rayResults->count; ++i) {
                                const auto& hit = rayResults->hits[i];
                                // Version 0.0.62 BOLT WALLS: the hands' aim walls, a shot's and the beam's
                                // (world space, as cast).
                                if (newBatch && hit.ray.tag < 2)
                                    handWallsWorld[hit.ray.tag] = bolt_walls::fromHit(hit.ray.origin, hit.ray.direction, hit.ray.distance,
                                                                                      native_rays::fixedSurface(hit), hit.fraction);
                                // Version 0.0.72 PUNISHER VAN SUMMON: the probe's answer (anything hit, a body too).
                                if (hit.ray.tag >= vanTag0 && hit.ray.tag < vanTag0 + vanTags) {
                                    if (newBatch && vanAsking && hit.ray.tag == vanAskTag && vanProbe.active) {
                                        vanAsking = false;
                                        ++vanAnswers;
                                        vanProbe.answer(hit.count > 0 && std::isfinite(hit.fraction), hit.fraction);
                                        if (!vanProbe.active) {
                                            vanSpot = vanProbe.result;
                                            vanSpotReady = true;
                                        }
                                    }
                                    continue;
                                }
                                if (hit.ray.tag >= bolt_walls::ballTag0) {
                                    int slot{};
                                    uint32_t low{};
                                    if (newBatch && bolt_walls::ballOfTag(hit.ray.tag, slot, low)) {
                                        const auto& b = oursBalls.ball(slot);
                                        if (b.alive && (b.serial & 15u) == low) {
                                            const auto w = bolt_walls::fromHit(hit.ray.origin, hit.ray.direction, hit.ray.distance,
                                                                               native_rays::fixedSurface(hit), hit.fraction);
                                            if (w.known) {
                                                shotWallsWorld[slot] = w;
                                                shotWallSerial[slot] = b.serial;
                                                ++wallAnswers;
                                                wallBlocked += w.blocked ? 1 : 0;
                                            }
                                        }
                                    } else if (newBatch && bolt_walls::isBeamTag(hit.ray.tag) && beamWait.waiting &&
                                               (beamWait.serial & 15u) == hit.ray.tag - bolt_walls::beamTag0) {
                                        beamWait.wall = bolt_walls::fromHit(hit.ray.origin, hit.ray.direction, hit.ray.distance,
                                                                            native_rays::fixedSurface(hit), hit.fraction);
                                        ++wallAnswers;
                                        wallBlocked += beamWait.wall.blocked ? 1 : 0;
                                    }
                                    continue;
                                }
                                // Update 38: a sight ray - a fixed surface (a wall, not a body)
                                // more than a metre short of him blocks the sight.
                                if (hit.ray.tag >= 100 && hit.ray.tag < 100 + world_trace::enemySlots) {
                                    if (!newBatch)
                                        continue;
                                    for (auto& ask : sightAsks) {
                                        if (!ask.serial || ask.serial != rayResults->serial)
                                            continue;
                                        const bool blocked = native_rays::fixedSurface(hit) &&
                                                             std::isfinite(hit.fraction) &&
                                                             hit.fraction * hit.ray.distance < hit.ray.distance - 1.f;
                                        sightBlocked += blocked;
                                        const uint8_t sight =
                                            blocked ? combat_spidy::sightBlocked : combat_spidy::sightClear;
                                        bool kept{};
                                        for (auto& seen : sightSeen)
                                            if (seen.id == ask.id) {
                                                seen.sight = sight;
                                                kept = true;
                                            }
                                        if (!kept)
                                            sightSeen[sightSeenNext++ % sightSeen.size()] = {ask.id, sight};
                                        ask = {};
                                    }
                                    continue;
                                }
                                if (hit.ray.tag != 7)
                                    continue;
                                roofFresh = roofFresh || newBatch;
                                // The surface over the feet, from where the ray
                                // started a tenth of a metre up. A reading
                                // tighter than a crouch is not a roof - a step,
                                // a strut, a lip at his feet - and the head is
                                // left alone for it: the rule is for a space a
                                // player is inside, not for whatever his feet
                                // are against.
                                const float found = native_rays::fixedSurface(hit) && std::isfinite(hit.fraction)
                                                        ? hit.fraction * hit.ray.distance + .1f
                                                        : 0.f;
                                headroomSeen = found >= leastHeadroom + headClearance ? found : 0.f;
                            }
                        }
                    }
                    if (SM2_PORT_GAMEPLAY && raysStarted && motion.active && !flatScreen && controlsVisible &&
                        !swingStarted && !swingFailed) {
                        game_swing::Config swing;
                        swing.pid = config.pid;
                        swing.base = config.base;
                        swing.record = player.record;
                        swing.mover = player.mover;
                        swing.motionModule = config.motionModule;
                        swing.durationMs = moduleDuration;
                        swing.maxSpeed = values.swingSpeed;
                        swing.gravity = vr_settings::gravity(values.weight);
                        swing.grabKinds = !values.webGrab || !sampleGrab ? 0 : game_grab::movableKinds;
                        // Fail safe as the rays: the movement bridge's bytes or
                        // hooks refused, swinging stays off and VR goes on.
                        if (const auto code = startSwing ? startSwing(&swing) : 1u) {
                            swingFailed = true;
                            const auto text = "Native swinging is off: " + gameplayRefusal(code) + " (" +
                                              std::to_string(code) + "); VR continues";
                            message(3, code, text.c_str());
                        } else {
                            swingStarted = true;
                            // Fists and the web shooter take the swing's input samples;
                            // both are optional and, like the grab, can be switched on later.
                            applySettings();
                        }
                    }
                    if (swingStarted) {
                        game_swing::Command input;
                        input.wallRotate = motion.wallRotate;
                        input.serial = ++serial;
                        input.focused = motion.active && !flatScreen && controlsVisible;
                        input.sampleTimeNs = frame.predictedDisplayTime > 0 ? frame.predictedDisplayTime : 1;
                        input.sampleSeconds =
                            std::isfinite(frame.seconds) && frame.seconds > 0 && frame.seconds <= .1f
                                ? frame.seconds
                                : 1.f / 90.f;
                        input.jump = motion.swing.jump;
                        input.move = motion.swing.move;
                        input.trackingYaw = motion.swing.trackingYaw;
                        input.tilt = motion.swing.tilt;
                        input.leap = input.focused ? leapOut : 0.f;
                        input.asks = input.focused && askingGame ? 1u : 0u;
                        input.asks |= hero_modes::leapAsks(heroNow()); // Version 0.0.6 HULK LEAP (CANDIDATE)
                        if (const Vec3 look = motion.headPose.orientation.rotate({0, 0, -1});
                            input.focused && finite(look) && length(look) > .5f)
                            input.look = normalized(look);
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& hand = motion.swing.hands[i];
                            // Update 37: a trigger held for a gadget gesture keeps its grip
                            // from shooting a swing web (Ricochet Web: grip + trigger).
                            // Update 38.2 GRAB WEB ENEMY: the swing button pressed with the web's ray on an
                            // enemy's capsule (last frame's WORLD LOG enemies, tracking space) holds the
                            // web on him - our rope, no swing web through him (the swing's webs never hit
                            // characters in SM2: game_swing.cpp notPorted) - until the button is let go;
                            // a pull toward the body is the yank (the Grabber's input, CANDIDATE).
                            {
                                const float squeeze = std::isfinite(hand.grip) ? hand.grip : 0.f;
                                const bool down = squeeze > (grabButtonWas[i] ? .3f : .6f);
                                const bool press = down && !grabButtonWas[i];
                                grabButtonWas[i] = down;
                                grabSqueeze[i] = down ? 1.f : 0.f;
                                // Update 38.3: 0 ASSIST (the 12-degree cone on the ground), 1 EXACT (38.2's
                                // ENEMY: the ray on the capsule), 2 OFF.
                                // 0.0.4 HERO ABILITIES: no GRAB WEB or ROPE GRAB for a hero with no webs.
                                const bool heroWebs = hero_modes::webPaths(heroNow()).ropeGrab;
                                const uint32_t grabMode = heroWebs ? c38(combat38::grabWeb) : 2u; // 2: OFF
                                // Version 0.0.3 ROPE GRAB (EXPERIMENTAL, OFF by default): the grip with the hand
                                // aimed at an enemy 3-9 m away (12-degree cone) tethers him to that hand; it takes
                                // the press before GRAB WEB and works with GRAB WEB OFF too.
                                const bool ropeOn = c38(combat38::ropeGrab) == 1 && heroWebs;
                                if (!ropeOn && ropes[i].on) {
                                    ropes[i].stop(rope_grab::letGo);
                                    grabWebs[i].release();
                                }
                                if (ropeOn && (ropes[i].on || press)) {
                                    if (press && hand.tracked && !ropes[i].on && frame.hands[i].valid &&
                                        validTrackedPose(frame.hands[i].grip)) {
                                        const Pose& g = frame.hands[i].grip;
                                        const Vec3 from = spidy::webShooter(g, i).position;
                                        const Vec3 ray = combat38::aimOf(g, frame.hands[i].aim, c38(combat38::aimAxis));
                                        std::array<Vec3, combat38::maxEnemies> chests{};
                                        const uint32_t count = std::min<uint32_t>(grabEnemyCount, combat38::maxEnemies);
                                        for (uint32_t e = 0; e < count; ++e)
                                            chests[e] = grabEnemies[e] + Vec3{0, combat38::centreMass, 0};
                                        const int e = count ? rope_grab::pick(from, ray, chests.data(), static_cast<int>(count)) : -1;
                                        if (e >= 0) {
                                            const auto at = static_cast<size_t>(e);
                                            ropes[i].attach(e, static_cast<uint32_t>(grabEnemyIds[at]), from, chests[at],
                                                            c38(combat38::ropeStrength));
                                            grabWebs[i].release();
                                            grabWebs[i].attach(e, chests[at]);
                                            grabIds[i] = grabEnemyIds[at];
                                            grabHandSet[i] = false;
                                            ropeHandSet[i] = false;
                                            if (ropes[i ^ 1u].on && ropes[i ^ 1u].id == ropes[i].id)
                                                ++ropeBothOnOne;
                                            runtime.haptic(static_cast<int>(i), .6f);
                                            game_prompts::note("Version 0.0.3: ROPE GRAB tether on an enemy (fling, slam or let go)");
                                        } else {
                                            ++ropeNoTarget;
                                        }
                                    }
                                } else if (grabMode == 2) {
                                    grabWebs[i].release();
                                } else if (press && hand.tracked && !grabWebs[i].on && frame.hands[i].valid &&
                                           validTrackedPose(frame.hands[i].grip) && grabEnemyCount) {
                                    const Pose& g = frame.hands[i].grip;
                                    const Vec3 from = spidy::webShooter(g, i).position;
                                    const Vec3 ray = combat38::aimOf(g, frame.hands[i].aim, c38(combat38::aimAxis));
                                    float metres = -1;
                                    bool assisted = false;
                                    const int e = combat38::grabAssistTarget(from, ray, grabEnemies, grabEnemyCount,
                                                                             grabMode == 0 && !grabAirborne, &metres,
                                                                             &assisted);
                                    if (e >= 0 && metres >= 0) {
                                        grabAssisted += assisted ? 1u : 0u;
                                        grabWebs[i].attach(e, from + normalized(ray) * metres);
                                        grabIds[i] = grabEnemyIds[static_cast<size_t>(e)];
                                        grabHandSet[i] = false;
                                        runtime.haptic(static_cast<int>(i), .6f);
                                        game_prompts::note("Update 38.2: GRAB WEB on an enemy (pull to yank)");
                                        if (c38(combat38::gadgetHint) == 0) { // Update 38.3 GADGET HINT
                                            gadgetHints[i].show(combat38::hintGrab);
                                            ++gadgetHintsShown;
                                        }
                                    }
                                }
                            }
                            input.hands[i] = {hand.aim, hand.gripRelativeToHead, hand.tracked, hand.trigger,
                                              u37.holdsGrip[i] || grabWebs[i].on ? 0.f : hand.grip};
                            input.wallPositions[i] = motion.grips[i].position;
                            // Update 37: climb_grip, the button that holds the wall.
                            input.wallGrips[i] =
                                hand.tracked ? hand_crawl::buttonOf({true, true, frame.hands[i].squeeze,
                                                                     frame.hands[i].trigger, {}, {}},
                                                                    climbGrip.load())
                                             : 0.f;
                        }
                        // Update 38 LUNGE SPIDYVR MOVE (EXPERIMENTAL): the bridge lunges itself, at the
                        // enemies near (world). Only with a ray module that has the lunge.
                        input.lungeMode = sampleLunge && (c38(combat38::lunge) == combat38::lungeSpidyMove ||
                                                          flyModeNow.load() != 0 || heroNow() != hero_modes::spiderMan)
                                              ? 1u
                                              : 0u;
                        for (uint32_t e = 0; e < enemiesHereCount && input.enemyCount < game_swing::lungeEnemies;
                             ++e) {
                            const Vec3 enemyWorld = motion.anchor + enemiesHere[e];
                            if (enemiesHereIds[e] && finite(enemyWorld))
                                input.enemies[input.enemyCount++] = {enemiesHereIds[e], enemyWorld, .3f};
                        }
                        // Update 38.5 FLY MODE (EXPERIMENTAL; extras385::fly): the speed, the triggers'
                        // thrust and the right stick's rise ride in the command's spare word. In the
                        // air the triggers and grips fly instead of shooting webs or web balls.
                        {
                            // Version 0.0.72 FLIGHT ON BY DEFAULT for the flyers (hero072::FlyDefault).
                            {
                                static hero072::FlyDefault flyDefault;
                                // Version 0.0.72 build 6 THOR FLIGHT OFF (a bisect key): THOR is no flyer then.
                                const bool thorGrounded = heroNow() == hero_modes::thor && c38(combat38::thorFlight) == 1;
                                const uint32_t want = flyDefault.update(thorGrounded ? hero_modes::hulk : heroNow(), flyModeNow.load());
                                if (want != flyModeNow.load()) {
                                    flyModeNow = want;
                                    game_prompts::note(want ? "Version 0.0.72: FLY MODE on (NORMAL) for this hero - a hand up takes off"
                                                            : "Version 0.0.72: FLY MODE back off (it was on for the last hero)");
                                }
                            }
                            const uint32_t fm = flyModeNow.load();
                            // Version 0.0.7 BATMAN: no flight at all (his cape glide and grapple instead).
                            const bool batmanNow = heroNow() == hero_modes::batman ||
                                                   (heroNow() == hero_modes::thor && c38(combat38::thorFlight) == 1); // build 6: THOR FLIGHT OFF
                            const bool flyLive = fm && !batmanNow && !calibrationInput && !padLayer.latched() && !phone.open();
                            // R3 drops and lands (FLY stays on; a hand up takes off again); L3+R3 within
                            // StickChord::window is still the game's ultimate.
                            const auto chord = flyChord.update(frame.seconds, flyLive && flyNow.flying,
                                                               frame.hands[0].stickClick, frame.hands[1].stickClick,
                                                               game_pad::pauseHoldSeconds);
                            const float dropDt = std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                            if (chord == extras385::fly::StickChord::drop) {
                                flyDropHeld = true;
                                flyDropSeconds = 0;
                                message(3, 0, "FLY: R3 - coming down to land (raise a hand to take off again)");
                            } else if (chord == extras385::fly::StickChord::ultimate) {
                                message(3, 0, "FLY: L3 + R3 - the ultimate");
                            }
                            if (flyDropHeld) {
                                flyDropSeconds += dropDt;
                                if (!flyLive || flyDropSeconds > 15.f || flyPoses.pose() == extras385::fly::rise ||
                                    (!flyNow.flying && flyDropSeconds > .5f))
                                    flyDropHeld = false;
                            }
                            // Version 0.0.7: a hero whose trigger fires a weapon (IRON MAN's repulsor, the
                            // batons, batarangs, claws, guns) never thrusts by it (hero_modes::flyThrust);
                            // the poses, the sticks and R3 fly him.
                            const uint32_t flyHero = heroNow();
                            input.reserved6 = flyLive ? extras385::fly::pack(fm, hero_modes::flyThrust(flyHero, input.hands[0].trigger),
                                                                            hero_modes::flyThrust(flyHero, input.hands[1].trigger),
                                                                            frame.hands[1].stickY, true,
                                                                            flyHero == hero_modes::greenLantern ? glPose : flyPoses.pose(),
                                                                            flyHero == hero_modes::greenLantern ? glLift : flyPoses.lift(), flyDropHeld)
                                                      : 0u;
                            // Version 0.0.5 SUPERSONIC: both hands out (FORWARD) and the left stick forward
                            // dash at SUPERSONIC SPEED x FAST; its first frame booms (sound + shockwave).
                            {
                                fly_boost::In bi;
                                bi.seconds = frame.seconds;
                                bi.on = flyLive && !flyDropHeld && c38(combat38::supersonic) == 0;
                                bi.flying = flyNow.flying != 0;
                                bi.handsOut = flyPoses.pose() == extras385::fly::forward;
                                bi.stickY = frame.hands[0].stickY;
                                const bool dash = flyBoost.update(bi);
                                // Version 0.0.6: the speed SUPERSONIC reached, logged as the dash ends.
                                if (dash && finite(swingState.velocity))
                                    sonicPeak = std::max(sonicPeak, length(swingState.velocity));
                                if (sonicWas && !dash) {
                                    sonicLast = sonicPeak;
                                    sonicBest = std::max(sonicBest, sonicPeak);
                                    char line[128];
                                    sprintf_s(line, "VERSION 0.0.6 SUPERSONIC: reached %.1f m/s (asked %.0f)", static_cast<double>(sonicPeak),
                                              static_cast<double>(fly_boost::speedOf(c38(combat38::supersonicSpeed))));
                                    game_prompts::note(line);
                                    sonicPeak = 0;
                                }
                                sonicWas = dash;
                                if (flyBoost.boomed()) {
                                    flyBoomAt = combatClock;
                                    sfx::play(sfx::sonicBoom);
                                    char line[96];
                                    sprintf_s(line, "VERSION 0.0.5 SUPERSONIC: boom %u, dash to %.0f m/s", flyBoost.engages(),
                                              static_cast<double>(fly_boost::speedOf(c38(combat38::supersonicSpeed))));
                                    game_prompts::note(line);
                                }
                                input.asks |= fly_boost::asksOf(dash, c38(combat38::supersonicSpeed));
                            }
                            if (flyLive && (flyNow.flying || game_swing::airborne(swingState, now.QuadPart,
                                                                                  frequency.QuadPart)))
                                for (auto& h : input.hands)
                                    h.trigger = h.grip = 0;
                            // Version 0.0.7 BATMAN (cape_glide.hpp): his glide is allowed, and the hands whose
                            // trigger the last hero step took as the grapple gun's (the bridge fires on the press).
                            const bool capeLive = batmanNow && !calibrationInput && !padLayer.latched() && !phone.open();
                            input.asks |= cape_glide::pack(capeLive, batmanGrapple[0], batmanGrapple[1]);
                            batmanGrapple[0] = batmanGrapple[1] = false;
                            // Version 0.0.72 GRAPPLE: DAREDEVIL and the PUNISHER grapple without the cape
                            // (askGrappleOnly every live frame, so the pull carries on to the point).
                            if (const uint32_t gh = heroNow(); gh != hero_modes::batman && grapple072::grappler(gh)) {
                                const bool grappleLive = !calibrationInput && !padLayer.latched() && !phone.open();
                                input.asks |= grapple072::asks(gh, grappleLive, hero072Grapple[0], hero072Grapple[1]);
                            }
                            hero072Grapple[0] = hero072Grapple[1] = false;
                            // Version 0.0.7: webs are SPIDER-MAN's only. Every other hero's triggers and grips
                            // never reach the bridge's webs, web balls or web grabs (climbing holds the wall by
                            // wallGrips, kept; FLY reads its own word).
                            if (heroNow() != hero_modes::spiderMan)
                                for (auto& h : input.hands)
                                    h.trigger = h.grip = 0;
                        }
                        if (const auto code = submitSwing(&input)) {
                            // 2002: the bridge switched itself off; its own
                            // error (SpidySwingSample keeps it) says why.
                            game_swing::Data last;
                            const uint32_t why =
                                code == 2002 && !sampleSwing(&last) && last.error ? last.error : code;
                            swingOff(why, why >= 2101 && why <= 2105
                                              ? "a world ray it cast returned a bad hit record"
                                              : code == 2002 ? "the swing bridge switched itself off"
                                                             : "the swing bridge refused the tracked input");
                        }
                        else if (const auto feedback = sampleSwing(&swingState))
                            swingOff(feedback, "the swing bridge's feedback could not be read");
                        else {
                            // Version 0.0.62 REAL ANCHORS: a web that met no world geometry within reach (the
                            // swing's own world ray, Swing::shot) misses; with webs in open air off, the hand
                            // that just shot gets a short buzz so the miss is felt. Air webs on: as before.
                            bool rising[2]{};
                            for (unsigned i = 0; i < 2; ++i) {
                                const bool shootNow = std::max(input.hands[i].trigger, input.hands[i].grip) > .5f;
                                rising[i] = shootNow && !anchorShootWas[i];
                                anchorShootWas[i] = shootNow;
                            }
                            if (swingState.misses < anchorMissesSeen)
                                anchorMissesSeen = swingState.misses; // the bridge restarted
                            if (swingState.misses > anchorMissesSeen) {
                                anchorMissesSeen = swingState.misses;
                                if (!values.airWebs) {
                                    const bool any = rising[0] || rising[1];
                                    for (unsigned i = 0; i < 2; ++i)
                                        if (rising[i] || !any)
                                            runtime.haptic(static_cast<int>(i), .15f, .06f);
                                    ++anchorMissBuzzes;
                                }
                            }
                        }
                        {
                            const uint32_t takeoffsBefore = flyNow.takeoffs;
                            if (!sampleFly || sampleFly(&flyNow))
                                flyNow = {};
                            if (flyNow.takeoffs > takeoffsBefore && takeoffsBefore) {
                                runtime.haptic(0, .3f);
                                runtime.haptic(1, .3f);
                            }
                        }
                        {   // Version 0.0.7 BATMAN: the cape glide's and the grapple's state (optional export).
                            const cape_glide::Data expected;
                            if (!sampleCape || sampleCape(&capeNow) || capeNow.magic != expected.magic ||
                                capeNow.version != expected.version || capeNow.bytes != expected.bytes) {
                                capeNow = {};
                            } else {
                                // Version 0.0.72: every grappler's (BATMAN, DAREDEVIL, the PUNISHER).
                                if (capeCounted && grapple072::grappler(heroNow())) {
                                    const unsigned h = capeNow.hand < 2 ? capeNow.hand : 1u;
                                    if (capeNow.grapples > capeGrapplesSeen) {
                                        runtime.haptic(static_cast<int>(h), .45f);
                                        sfx::play(sfx::batonWhoosh); // the grapple's fire (never a web sound)
                                        if (++hero072GrappleNotes <= 16) {
                                            char line[160];
                                            sprintf_s(line, "%s (%s)", grapple072::fireNote[static_cast<uint32_t>(grapple072::deviceOf(heroNow()))],
                                                      grapple072::heroName(heroNow()));
                                            game_prompts::note(line);
                                        }
                                    }
                                    if (capeNow.arrivals > capeArrivalsSeen) {
                                        runtime.haptic(static_cast<int>(h), .35f);
                                        sfx::play(sfx::holsterTick);
                                    }
                                    if (capeNow.misses > capeMissesSeen)
                                        for (int i = 0; i < 2; ++i)
                                            runtime.haptic(i, .12f, .05f);
                                    if (capeNow.glides > capeGlidesSeen) {
                                        runtime.haptic(0, .25f);
                                        runtime.haptic(1, .25f);
                                        game_prompts::note("VERSION 0.0.7 BATMAN: cape glide");
                                    }
                                }
                                capeCounted = true;
                                capeGrapplesSeen = capeNow.grapples;
                                capeMissesSeen = capeNow.misses;
                                capeGlidesSeen = capeNow.glides;
                                capeArrivalsSeen = capeNow.arrivals;
                            }
                            capeGlidingNow.store(capeNow.gliding != 0 && heroNow() == hero_modes::batman, std::memory_order_relaxed);
                        }
                        if (!sampleClimb || sampleClimb(&climbState) || climbState.magic != 0x53434c44 ||
                            climbState.version != 1 || climbState.bytes != sizeof(climbState))
                            climbState = {};
                        const unsigned wallPulses =
                            climbPulses.update(climbState, input.focused && values.handClimb);
                        for (unsigned i = 0; i < 2; ++i)
                            if (wallPulses & (1u << i)) {
                                runtime.haptic(i, .45f, .045f);
                                feel(body_haptics::Kind::wallGrip, static_cast<int>(i));
                            }
                        if (!sampleBends || sampleBends(&swingBends))
                            swingBends = {};
                        // Nothing here changes who holds the player on a wall
                        // behind his back. Every way of doing it from the room
                        // around him dropped him (October 14: switching the
                        // walls off, refusing the next wall of a climb, and a
                        // push into the wall with the walls off, each a fall of
                        // up to 7.5 m), and the player put the reason plainly:
                        // "the shaft shouldnt affect anything, the vent in the
                        // top is the target, there are similiar events in the
                        // game with vents outside". The room is still read, for
                        // the report.
                        if (swingStarted && swingState.error)
                            swingOff(swingState.error, swingState.error >= 2101 && swingState.error <= 2105
                                                           ? "a world ray it cast returned a bad hit record"
                                                           : "the swing bridge reported an error");
                        if (!sampleGrab || sampleGrab(&grabState))
                            grabState = {};
                        // What each free hand's web button would do: the aim markers.
                        aims = aimed = {};
                        if (sampleAim && input.focused) {
                            game_swing::AimData sample;
                            const game_swing::AimData expected;
                            if (!sampleAim(&sample) && sample.magic == expected.magic &&
                                sample.version == expected.version && sample.bytes == expected.bytes &&
                                sample.status == 2)
                                aimed = sample;
                        }
                        if (values.aimMarkers && heroNow() == hero_modes::spiderMan) // 0.0.7: web aims are his only
                            aims = aimed;
                        // A free hand come onto the other hand's web: a light
                        // tick, a press here takes hold of that web.
                        for (unsigned i = 0; i < 2; ++i) {
                            const bool on =
                                aimed.hands[i].kind == static_cast<uint32_t>(game_swing::AimKind::strand);
                            if (on && !onStrandBefore[i] &&
                                frame.predictedDisplayTime - strandTickAt[i] > 400'000'000) {
                                runtime.haptic(i, .2f);
                                strandTickAt[i] = frame.predictedDisplayTime;
                            }
                            onStrandBefore[i] = on;
                        }
                        bool trails[2]{};
                        Vec3 trailEnds[2]{};
                        // The hands whose webs a zip pulled: one zip on the body, in the arm of
                        // each (both: a web held in both hands).
                        unsigned zipHands{};
                        for (unsigned i = 0; i < 2; ++i) {
                            // A web that caught a thug ends on him, wherever he goes.
                            const auto& held = grabState.hands[i];
                            const bool grabbing = input.focused && held.phase != 0;
                            const bool wasGrabbing = grabPhases[i] != 0;
                            const int64_t displayAt = frame.predictedDisplayTime;
                            if (grabbing) {
                                swingState.webs[i] = {1, 0, held.end, held.length, held.taut ? 1.f : 0.f};
                                // Caught, yanked, at the hand: each its own pulse.
                                const auto phase = static_cast<GrabPhase>(held.phase);
                                if (held.phase != grabPhases[i]) {
                                    runtime.haptic(i, phase == GrabPhase::Yanked ? 1.f
                                                      : phase == GrabPhase::Held ? .4f
                                                                                 : .55f);
                                    pulseUntil[i] = displayAt + 30'000'000;
                                    feel(phase == GrabPhase::Yanked ? body_haptics::Kind::grabYanked
                                         : phase == GrabPhase::Held ? body_haptics::Kind::grabHeld
                                                                    : body_haptics::Kind::grabCaught,
                                         static_cast<int>(i));
                                } else if (held.tension > .05f && displayAt >= pulseUntil[i]) {
                                    // Between pulses the hand feels the web pull: a
                                    // hanging trash can faintly, one swung hard or too
                                    // heavy to lift strongly.
                                    runtime.haptic(i, std::min(.6f, .6f * held.tension));
                                }
                            } else if (wasGrabbing) {
                                runtime.haptic(i, .6f); // thrown or let go
                                feel(body_haptics::Kind::grabLetGo, static_cast<int>(i));
                            }
                            grabPhases[i] = grabbing ? held.phase : 0;
                            // The web let go of trails what it held as it dissolves.
                            trails[i] = input.focused && !grabbing && held.trailing;
                            trailEnds[i] = held.end;
                            const bool attached =
                                input.focused && swingState.status && swingState.webs[i].attached;
                            // A web taken hold of, not shot: the hand closes on it,
                            // a softer pulse than a shot's.
                            const bool taken = attached && !grabbing && (swingState.webs[i].attached & 4);
                            if (attached != attachedBefore[i] && !grabbing && !wasGrabbing) {
                                runtime.haptic(i, !attached ? .2f : taken ? .4f : .65f);
                                feel(!attached ? body_haptics::Kind::webRelease
                                     : taken   ? body_haptics::Kind::webTaken
                                               : body_haptics::Kind::webAttach,
                                     static_cast<int>(i));
                            }
                            if (attached && swingState.zips > zipBefore && !grabbing) {
                                runtime.haptic(i, 1);
                                zipHands |= 1u << i;
                            }
                            attachedBefore[i] = attached;
                            webTaken[i] = taken;
                            webTimes.update(i, attached, swingState.webs[i].anchor,
                                            webStart(motion.grips[i], i, webStartSpot.load()),
                                            frame.predictedDisplayTime);
                        }
                        zipBefore = swingState.zips;
                        if (zipHands) {
                            // On the body toward where the zipped webs pull it.
                            Vec3 toward{};
                            for (unsigned i = 0; i < 2; ++i)
                                if (zipHands & (1u << i))
                                    toward += swingState.webs[i].anchor - swingState.position;
                            feelFrom(body_haptics::Kind::zip, zipHands == 3 ? -1 : zipHands == 2 ? 1 : 0, 1, toward);
                        }
                        {
                            // One web in both hands is one of the game's ropes,
                            // from the hand that holds it nearer its end.
                            const bool shared = attachedBefore[0] && attachedBefore[1] &&
                                                (swingState.webs[0].attached & 2) &&
                                                (swingState.webs[1].attached & 2) && !grabPhases[0] && !grabPhases[1];
                            std::array<WebRopes::Hand, 2> held{};
                            for (unsigned i = 0; i < 2; ++i)
                                held[i] = {attachedBefore[i], webTaken[i], shared, swingState.webs[i].length,
                                           webTimes[i].attachedAt};
                            webRopes.update(held);
                        }
                        // A thug the web brought to a player in the air has
                        // arrived within reach: time slows, to hit him there.
                        // (Until the fifth build of October 12 it slowed as the
                        // yank began, with the thug still far off.)
                        if (grabState.catches > catchesBefore && input.focused &&
                            game_swing::airborne(swingState, now.QuadPart, frequency.QuadPart)) {
                            moments.start(Moment::airPull);
                            runtime.haptic(0, .45f);
                            runtime.haptic(1, .45f);
                            feel(body_haptics::Kind::airPull);
                        }
                        catchesBefore = grabState.catches;
                        // Onto a wall, and off it with a jump: both hands feel it.
                        if (input.focused && swingState.status &&
                            (swingState.walls > wallsBefore || swingState.wallJumps > wallJumpsBefore)) {
                            const float pulse = swingState.wallJumps > wallJumpsBefore ? .6f : .45f;
                            runtime.haptic(0, pulse);
                            runtime.haptic(1, pulse);
                            // On the body's side toward the wall (behind its normal).
                            feelFrom(swingState.wallJumps > wallJumpsBefore ? body_haptics::Kind::wallJump
                                                                            : body_haptics::Kind::wallLand,
                                     -1, 1, -swingState.wallNormal);
                        }
                        wallsBefore = swingState.walls;
                        wallJumpsBefore = swingState.wallJumps;
                        // Every landed blow, a fist's or a web strike's: the hand
                        // that threw it kicks, as hard and as long as the blow was
                        // (a jab 60 ms, a blow that sends him flying 180 ms and the
                        // other hand with it); the game's time stops for a moment;
                        // a burst shows where it landed, with its count in the
                        // flow; and it sounds. Until October 11 a landed punch was
                        // one 25 ms pulse at half strength and nothing else.
                        game_punch::Strikes strikes{};
                        const game_punch::Strikes expectedStrikes;
                        const bool fighting =
                            punchStarted && sampleStrike && !sampleStrike(&strikes) &&
                            strikes.magic == expectedStrikes.magic && strikes.version == expectedStrikes.version &&
                            strikes.bytes == expectedStrikes.bytes;
                        if (fighting) {
                            // Punching switched off and on again counts from nothing.
                            blowsBefore = std::min(blowsBefore, strikes.blows);
                            strikesBefore = std::min(strikesBefore, strikes.opened);
                            knockoutsBefore = std::min(knockoutsBefore, strikes.knockouts);
                            if (strikes.opened > strikesBefore && input.focused) {
                                // Off to him: a tick in both hands, and the rush.
                                runtime.haptic(0, .35f, .05f);
                                runtime.haptic(1, .35f, .05f);
                                feel(body_haptics::Kind::strikeRush);
                                play(ImpactSound::Rush);
                            }
                            if (strikes.blows > blowsBefore && input.focused) {
                                const auto kind = static_cast<BlowKind>(strikes.lastKind);
                                const bool called = strikes.lastCalled != 0;
                                const bool finisher = kind == BlowKind::Finisher,
                                           heavy = kind != BlowKind::Hit || called;
                                // Time slows for a moment at the blows worth a look.
                                if (called)
                                    moments.start(Moment::called);
                                else if (finisher || kind == BlowKind::Launch || kind == BlowKind::Slam)
                                    moments.start(Moment::blow);
                                const unsigned hand = strikes.lastHand & 1;
                                const float strength =
                                    heavy ? 1.f
                                          : std::isfinite(strikes.lastStrength)
                                                ? std::clamp(strikes.lastStrength, 0.f, 1.f)
                                                : 0.f;
                                const float kick = finisher ? .32f : heavy ? .18f : .06f + .08f * strength;
                                runtime.haptic(hand, heavy ? 1.f : .6f + .4f * strength, kick);
                                if (heavy)
                                    runtime.haptic(1 - hand, finisher ? 1.f : .5f, finisher ? kick : .1f);
                                feel(finisher ? body_haptics::Kind::finisher
                                     : heavy  ? body_haptics::Kind::heavyBlow
                                              : body_haptics::Kind::punch,
                                     static_cast<int>(hand), strength);
                                for (unsigned i = 0; i < 2; ++i)
                                    if (i == hand || heavy)
                                        pulseUntil[i] = frame.predictedDisplayTime +
                                                        static_cast<int64_t>(kick * 1e9f) + 20'000'000;
                                hitStop.hit(strength, finisher);
                                if (finite(strikes.lastPoint)) {
                                    auto& burst = bursts[nextBurst++ % bursts.size()];
                                    burst.view = {strikes.lastPoint, strength, kind,
                                                  static_cast<int>(std::min(strikes.lastCount, 999u)), 0, called};
                                    burst.since = frame.predictedDisplayTime;
                                    // Update 37: BLOOD SPLATTER on a CONFIRMED landed blow (not ported in SM2).
                                    if (const uint32_t blood = bloodIntensity.load())
                                        bloodPool.fire(strikes.lastPoint, strikes.lastPoint - bloodViewer, blood, true);
                                }
                                play(finisher ? ImpactSound::Finisher
                                     : heavy ? ImpactSound::Heavy
                                     : strength >= .35f ? ImpactSound::Firm
                                                        : ImpactSound::Light);
                            }
                            // A knockout of the player's: time eases for an instant, both
                            // hands feel it, and it is lettered where he stood. This one is
                            // done.
                            if (strikes.knockouts > knockoutsBefore && input.focused &&
                                finite(strikes.knockoutPoint)) {
                                moments.start(Moment::knockout);
                                runtime.haptic(0, .8f, .09f);
                                runtime.haptic(1, .8f, .09f);
                                feel(body_haptics::Kind::knockout);
                                auto& mark = knockoutMarks[nextKnockout++ % knockoutMarks.size()];
                                mark.view = {strikes.knockoutPoint, 0};
                                mark.since = frame.predictedDisplayTime;
                            }
                            blowsBefore = strikes.blows;
                            strikesBefore = strikes.opened;
                            knockoutsBefore = strikes.knockouts;
                        }
                        // The mark over the thug the next punch would go to, with
                        // the arrow of the blow the flow calls. A new call ticks
                        // in both hands.
                        if (fighting && input.focused && strikes.cueTarget && finite(strikes.cuePoint)) {
                            const bool locked = strikes.cueLocked != 0;
                            if (strikes.cueTarget != cueTarget || strikes.cueWay != cueWay || locked != cueLocked)
                                cueSince = frame.predictedDisplayTime;
                            cueLocked = locked;
                            if (strikes.cueWay && strikes.cueWay != cueWay) {
                                runtime.haptic(0, .12f);
                                runtime.haptic(1, .12f);
                            }
                            cueTarget = strikes.cueTarget;
                            cueWay = strikes.cueWay;
                            cue = {strikes.cuePoint, static_cast<CueWay>(strikes.cueWay), strikes.cueFinisher != 0,
                                   strikes.cueLeft, 0, locked};
                        } else {
                            cue = {};
                            cueTarget = 0;
                            cueWay = 0;
                            cueLocked = false;
                        }
                        // Spider-Sense (spider_sense.hpp): a mark for each attacker the
                        // game warned the player of, from the display time it showed.
                        // A new one tingles in the hand on his side (both for one
                        // ahead or behind), a dodge's harder. A counter breaks its
                        // mark; time slows for a moment and both hands feel it.
                        game_sense::Data senseNow{};
                        const game_sense::Data expectedSense;
                        const bool sensing = punchStarted && sampleSense && !sampleSense(&senseNow) &&
                                             senseNow.magic == expectedSense.magic &&
                                             senseNow.version == expectedSense.version &&
                                             senseNow.bytes == expectedSense.bytes && senseNow.status == 2;
                        for (auto& mark : senseMarks)
                            mark.live = false;
                        if (sensing && input.focused) {
                            const Pose& eyes = motion.headPose;
                            for (uint32_t i = 0; i < std::min<uint32_t>(senseNow.count, 6); ++i) {
                                const auto& t = senseNow.threats[i];
                                if (!t.attacker || !finite(t.head) || !t.answer || t.answer > 3)
                                    continue;
                                SenseMark* mark{};
                                for (auto& m : senseMarks)
                                    if (m.attacker == t.attacker)
                                        mark = &m;
                                if (!mark) {
                                    for (auto& m : senseMarks)
                                        if (!m.attacker && !mark)
                                            mark = &m;
                                    if (!mark)
                                        continue;
                                    *mark = {};
                                    mark->attacker = t.attacker;
                                    mark->since = frame.predictedDisplayTime;
                                    ++senseShown;
                                    const Vec3 to = t.head - eyes.position;
                                    const Vec3 right = eyes.orientation.rotate({1, 0, 0});
                                    const bool placed = length(to) > 1e-3f && validTrackedPose(eyes);
                                    const float side = placed ? dot(normalized(to), right) : 0.f;
                                    const bool dodge = t.answer == static_cast<uint8_t>(SenseAnswer::Dodge);
                                    const int64_t until = frame.predictedDisplayTime + 90'000'000;
                                    for (unsigned h = 0; h < 2; ++h)
                                        if ((h == 0 && side < .35f) || (h == 1 && side > -.35f)) {
                                            runtime.haptic(h, dodge ? .6f : .35f, dodge ? .07f : .045f);
                                            pulseUntil[h] = std::max(pulseUntil[h], until);
                                        }
                                    ++senseTingles;
                                    // In the head, from his side.
                                    feelFrom(dodge ? body_haptics::Kind::senseDodge : body_haptics::Kind::spiderSense,
                                             -1, 1, placed ? to : Vec3{});
                                }
                                mark->live = true;
                                mark->head = t.head;
                                mark->answer = static_cast<SenseAnswer>(t.answer);
                                mark->left = std::isfinite(t.left) ? std::clamp(t.left, 0.f, 1.f) : 1.f;
                                mark->timed = t.timed != 0;
                                if (t.countered && !mark->brokenAt) {
                                    mark->brokenAt = frame.predictedDisplayTime;
                                    ++senseBroken;
                                }
                            }
                            // The attack to land first, timed ones before the rest: a
                            // blow the player takes next comes from him (the hits below).
                            const game_sense::Threat* first{};
                            for (uint32_t i = 0; i < std::min<uint32_t>(senseNow.count, 6); ++i) {
                                const auto& t = senseNow.threats[i];
                                if (t.attacker && finite(t.middle) &&
                                    (!first || (t.timed && (!first->timed || t.toImpact < first->toImpact))))
                                    first = &t;
                            }
                            if (first) {
                                blowFrom = first->middle;
                                blowSeen = frame.predictedDisplayTime;
                            }
                            // A counter made: its attacker's mark breaks, even one the
                            // game has taken off already.
                            const uint64_t counters = senseNow.thrown + senseNow.landed;
                            countersBefore = std::min(countersBefore, counters);
                            if (counters > countersBefore) {
                                for (auto& m : senseMarks)
                                    if (m.attacker && m.attacker == senseNow.lastCounter && !m.brokenAt) {
                                        m.brokenAt = frame.predictedDisplayTime;
                                        ++senseBroken;
                                    }
                                moments.start(Moment::called);
                                const int64_t until = frame.predictedDisplayTime + 100'000'000;
                                for (unsigned h = 0; h < 2; ++h) {
                                    runtime.haptic(h, .8f, .08f);
                                    pulseUntil[h] = std::max(pulseUntil[h], until);
                                }
                                feel(body_haptics::Kind::counter);
                            }
                            countersBefore = counters;
                        }
                        // A warning gone (its blow landed, or the game took it off) is
                        // gone at once; a broken mark plays out its poses first.
                        for (auto& m : senseMarks) {
                            const double sinceBroken =
                                static_cast<double>(frame.predictedDisplayTime - m.brokenAt) * 1e-9;
                            const bool breaking = m.brokenAt && sinceBroken < SenseView::brokenSeconds;
                            if (m.attacker && !m.live && !breaking)
                                m = {};
                        }
                        game_punch::Data punch{};
                        if (punchStarted && !samplePunch(&punch))
                            for (unsigned i = 0; i < 2; ++i) {
                                // With an older ray module: a landed punch knocks back
                                // into the hand, as hard as it was; the web's rumble
                                // waits for it.
                                if (!fighting && punch.hands[i].punches > punchesBefore[i] && input.focused) {
                                    runtime.haptic(i, .5f + .5f * punch.hands[i].lastStrength);
                                    pulseUntil[i] = frame.predictedDisplayTime + 40'000'000;
                                    feel(body_haptics::Kind::punch, static_cast<int>(i), punch.hands[i].lastStrength);
                                }
                                punchesBefore[i] = punch.hands[i].punches;
                            }
                        game_shooter::Data shots{};
                        if (shooterStarted && !sampleShooter(&shots)) {
                            listed = shots;
                            for (unsigned i = 0; i < 2; ++i) {
                                // A web ball leaving the wrist: a short, crisp tick.
                                if (shots.hands[i].shots > shotsBefore[i] && input.focused) {
                                    runtime.haptic(i, .35f);
                                    pulseUntil[i] = frame.predictedDisplayTime + 25'000'000;
                                    ballShot[i] = true;
                                    feel(body_haptics::Kind::webShot, static_cast<int>(i));
                                }
                                shotsBefore[i] = shots.hands[i].shots;
                            }
                        }
                        fightNear = input.focused && ((fighting && strikes.cueTarget != 0) || punch.bots != 0);
                        if (websStarted && motion.active && !flatScreen) {
                            // The game draws these webs from the tracked web shooters.
                            native_webs::Request webs;
                            native_webs::Bent bent[2]{};
                            webs.feet = motion.anchor;
                            for (unsigned i = 0; i < 2; ++i) {
                                auto& hand = webs.hands[i];
                                // Each rope draws the web WebRopes gave it, from the
                                // web shooter of the hand that holds that web nearer
                                // its end.
                                const auto& rope = webRopes.rope(i);
                                const auto from =
                                    static_cast<unsigned>(rope.hands ? rope.from : rope.last >= 0 ? rope.last : i);
                                // A web let go of a thrown prop goes with it as it dissolves.
                                const bool trail = !rope.hands && rope.last >= 0 && !attachedBefore[from] && trails[from];
                                hand.attached = rope.hands ? 1 : trail ? 2 : 0;
                                hand.tracked = motion.swing.hands[from].tracked;
                                hand.attachedAt = rope.key;
                                hand.anchor = trail ? trailEnds[from] : swingState.webs[from].anchor;
                                hand.wrist = webStart(motion.grips[from], from, webStartSpot.load());
                                // A web round edges: to where it sticks, round each of them. The
                                // swing's anchor is the nearest edge, and drawn to it the web's end
                                // jumped to each new one. Where Spidy cannot lay the rope's points
                                // (no hand points hook) it stays drawn to that edge, not straight
                                // through the building to its end.
                                if (const auto& edges = swingBends.hands[from];
                                    hand.attached == 1 && !grabPhases[from] && edges.count &&
                                    finite(edges.end) && native_webs::drawsHeld()) {
                                    hand.anchor = edges.end;
                                    bent[i].count =
                                        std::min<uint32_t>(edges.count, native_webs::maxRopeBends);
                                    for (uint32_t k = 0; k < bent[i].count; ++k)
                                        bent[i].points[k] = edges.points[k];
                                }
                            }
                            // One web in both hands: its rope comes to the upper
                            // wrist from the lower one, in the game's own material.
                            if (const int upper = webRopes.upper(); upper >= 0) {
                                const auto lower = static_cast<unsigned>(1 - upper);
                                const int slot = webRopes.slot(lower);
                                if (slot >= 0 && motion.swing.hands[lower].tracked) {
                                    const Vec3 wrist = webStart(motion.grips[lower], lower, webStartSpot.load());
                                    webs.held.rope = static_cast<uint32_t>(slot) + 1;
                                    webs.held.wrist = wrist;
                                    webs.held.slack =
                                        std::max(swingState.webs[lower].length - swingState.webs[upper].length -
                                                     length(webs.hands[slot].wrist - wrist),
                                                 0.f);
                                }
                            }
                            native_webs::submit(webs, 200, bent);
                        }
                    }
                    const bool steering = motion.active && !flatScreen && controlsVisible;
                    motion.nativeKeys = airJump.update(
                        motion.nativeKeys, game_swing::airborne(swingState, now.QuadPart, frequency.QuadPart));
                    perfSections.mark(perf37::combat); // Update 37: where the CPU time goes
                    // Going up onto a wall (game_swing::Data::mount), the game
                    // gets the jump alone: the stick walked the player on into
                    // the wall, and into the game's own crawl.
                    const bool mounting =
                        swingState.status && swingState.mount && swingState.qpc &&
                        static_cast<int64_t>(swingState.qpc) > now.QuadPart - frequency.QuadPart / 10;
                    auto keys = swingNativeKeys(
                        steering, swingState.owned != 0 || mounting, motion.nativeKeys,
                        static_cast<SwingTakeoff::Phase>(swingState.takeoffPhase), swingState.takeoff != 0);
                    // A held on the ground charges a leap, the swing's own, let
                    // go of with A. A tap is the jump as before, made as it is
                    // let go. The swing's own jumps (a takeoff, a wall, the
                    // leap's) are the game's A as they are.
                    const bool standing = steering && padInstalled && swingState.status && swingState.grounded &&
                                          !swingState.owned && !mounting && !swingState.takeoff &&
                                          !swingState.takeoffPhase && !swingState.webs[0].attached &&
                                          !swingState.webs[1].attached && !calibrationInput;
                    const bool aNow = (keys & swingJumpKey) && !swingState.takeoff;
                    if (aNow && !aWas72 && !standing && steering) {
                        // Build 5: why a press is a plain jump, not a charge (the console, at most every 2 s).
                        static ULONGLONG saidAt = 0;
                        if (GetTickCount64() - saidAt > 2000) {
                            saidAt = GetTickCount64();
                            char why[160];
                            sprintf_s(why, "JUMP: A is a plain jump (no charge): grounded %u owned %u takeoff %u phase %u webs %u%u pad %u",
                                      swingState.grounded ? 1u : 0u, swingState.owned ? 1u : 0u, swingState.takeoff ? 1u : 0u,
                                      static_cast<unsigned>(swingState.takeoffPhase), swingState.webs[0].attached ? 1u : 0u,
                                      swingState.webs[1].attached ? 1u : 0u, padInstalled ? 1u : 0u);
                            consoleLine(why);
                        }
                    }
                    aWas72 = aNow;
                    const auto leap = chargeJump.update(frame.seconds, aNow, standing);
                    if (!swingState.takeoff)
                        keys = (keys & ~swingJumpKey) | (leap.jump ? swingJumpKey : 0u);
                    leapOut = leap.leap;
                    // Build 5: DAREDEVIL's baton cast waits while A is held, a leap charges or goes out,
                    // or the swing takes off (a cast then cost the leap its velocity: a plain jump).
                    jumpBusy72 = (keys & swingJumpKey) || chargeJump.charging() || leap.leap > 0 ||
                                 swingState.takeoff || swingState.takeoffPhase;
                    if (chargeJump.charges() != chargesBefore) {
                        chargesBefore = chargeJump.charges();
                        runtime.haptic(1, .3f);
                        feel(body_haptics::Kind::leapCharge);
                    } else if (chargeJump.charging()) {
                        // It builds in both hands, and holds at full.
                        const float charge = chargeJump.charge();
                        runtime.haptic(0, .08f + .3f * charge);
                        runtime.haptic(1, .1f + .4f * charge);
                    }
                    if (chargeJump.leaps() != leapsBefore) {
                        leapsBefore = chargeJump.leaps();
                        runtime.haptic(0, .8f, .12f);
                        runtime.haptic(1, 1.f, .12f);
                        feel(body_haptics::Kind::leap);
                    }
                    controls(keys);
                    askingGame = false;
                    if (padInstalled) {
                        // The same walking and jumping on the virtual Xbox
                        // controller: after the VR menus, the game plays the
                        // player with it and ignores the bridge's keys.
                        game_pad::Walk walk;
                        // Update 37: live.txt wall_crawl and hand_climb, taken here.
                        if (wallCrawlChanged.exchange(false))
                            sendSwingSettings();
                        if (const int asked = handClimbAsked.exchange(-1); asked >= 0 && (asked != 0) != values.handClimb) {
                            values.handClimb = asked != 0;
                            applySettings();
                        }
                        const bool playing = steering && !swingState.owned;
                        // Reaching for a thing is not walking: a player standing on a
                        // wall does it as much as one on the ground. The swing owns
                        // him there (it has taken the walls since October 10), so
                        // `playing` was false for the whole climb and the game's Y
                        // never went out. The user, October 13, at a closed vent
                        // reached by walking up the wall it is on: "its right in from
                        // of me and its closed and i cant iteract with it". That
                        // session's report has the swing owning the player in every
                        // one of its 1106 samples on a wall, none of them grounded,
                        // and the game's Y in 2 of 4230 samples.
                        const bool onWall = swingState.wall != 0;
                        const bool reaching =
                            steering && (!swingState.owned || onWall) && !calibrationInput;
                        // And in flight, the swing carrying him: the game's zip to a
                        // point (the stick up) and its web strike (Y, a car's in a
                        // chase) go out there too since October 18, and the swing
                        // lets the game have him once its move has begun
                        // (game_swing::AirYield). Held back until then: a hand-over
                        // in mid-flight dropped players at the game's fall speed.
                        const bool inFlight = steering && swingState.owned && !onWall && !calibrationInput;
                        gestureInFlight = inFlight; // Update 38.5 GESTURE MOVES' airborne (next frame)
                        // Clicking both thumbsticks latches the whole pad, for the
                        // controls play has no room for (the game's X, its L3, each
                        // bumper and trigger on its own, three ways of its D-pad).
                        // Neither stick's own press is half of the chord (PullPress,
                        // slow_motion's StickPress both drop one), so nothing else
                        // answers it. A press of any kind keeps the layer open.
                        const bool layerBusy =
                            frame.buttons != 0 || frame.hands[0].squeeze > .6f ||
                            frame.hands[1].squeeze > .6f || frame.hands[0].trigger > .6f ||
                            frame.hands[1].trigger > .6f ||
                            std::hypot(frame.hands[1].stickX, frame.hands[1].stickY) > .5f;
                        bool layerOn =
                            padLayer.update(frame.seconds, playing && !calibrationInput,
                                            frame.hands[0].stickClick, frame.hands[1].stickClick, layerBusy);
                        // Update 38.5 PAD MODE (the holo's button) and PROMPT HANDLING AUTO: never in a
                        // fight (the combat camera), never while a web carries you; AUTO needs the
                        // game's prompts hooked (prompts::hooked, 9699 in SM2 so far).
                        {
                            const bool padAllowed = playing && !calibrationInput;
                            if (padModePressed) {
                                padModePressed = false;
                                padLayer.press(padAllowed);
                            }
                            const auto prompted = game_prompts::demand();
                            const bool autoOn = extras385::prompts::hooked &&
                                                promptHandlingNow.load() == extras385::prompts::automatic;
                            const bool inFight = moverTable == config.base + bridge::combatCamera;
                            padLayer.prompt(frame.seconds, autoOn, padAllowed && !inFight,
                                            game_pad::promptNeedsPad(prompted.buttons, prompted.leftTrigger,
                                                                     prompted.rightTrigger));
                            layerOn = padLayer.latched();
                            if (padLayer.autoEntries() != autoEntriesBefore) {
                                autoEntriesBefore = padLayer.autoEntries();
                                padEntriesBefore = padLayer.entries(); // AUTO's own line, below
                                runtime.haptic(0, .3f);
                                runtime.haptic(1, .3f);
                                message(3, 0, extras385::prompts::hint);
                                promptHinted = true;
                            }
                        }
                        if (padLayer.entries() != padEntriesBefore) {
                            padEntriesBefore = padLayer.entries();
                            runtime.haptic(0, .5f);
                            runtime.haptic(1, .5f);
                            message(3, 0, "Pad layer on - your grips are the game's LB and RB, your triggers its "
                                          "LT and RT, X its X, the right stick its D-pad; click both sticks "
                                          "again to leave it");
                        } else if (padLayerWas && !layerOn) {
                            runtime.haptic(0, .25f);
                            runtime.haptic(1, .25f);
                            message(3, 0, "Pad layer off - the webs are back on your grips and triggers");
                        }
                        padLayerWas = layerOn;
                        if (playing) {
                            walk.right = mounting ? 0 : motion.walkRight;
                            walk.forward = mounting ? 0 : motion.walkForward;
                            // Update 37: hand-over-hand on the game's own crawl: a hand
                            // holding near the wall and pulling is the crawl's stick
                            // (hand_crawl.hpp; the stick stays plan B, it works as before).
                            {
                                hand_crawl::Frame cf;
                                cf.seconds = frame.seconds;
                                cf.play = !calibrationInput && !layerOn && !mounting;
                                cf.crawl = hold.crawl;
                                cf.normal = hold.normal;
                                cf.anchor = hold.anchor;
                                cf.forwardAxis = motion.walkForwardAxis;
                                cf.turn = trackingTurn(motion.swing);
                                for (unsigned i = 0; i < 2; ++i) {
                                    auto& h = cf.hands[i];
                                    h.tracked = motion.swing.hands[i].tracked;
                                    h.free = !swingState.webs[i].attached && !grabPhases[i] && !u37.holdsGrip[i];
                                    h.grip = frame.hands[i].squeeze;
                                    h.trigger = frame.hands[i].trigger;
                                    h.relative = motion.swing.hands[i].gripRelativeToHead;
                                    h.world = motion.grips[i].position;
                                }
                                hand_crawl::Config cc;
                                cc.on = climbOn.load();
                                cc.pull = climbPull.load();
                                cc.grip = climbGrip.load();
                                const auto climb = climber.update(cf, cc);
                                for (unsigned i = 0; i < 2; ++i) {
                                    u37.climbing[i] = climb.holding[i];
                                    if (climb.grabbed[i]) {
                                        runtime.haptic(static_cast<int>(i), .3f);
                                        feel(body_haptics::Kind::wallGrip, static_cast<int>(i), .4f);
                                    }
                                }
                                if (climb.active) {
                                    walk.right = climb.stickX;
                                    walk.forward = climb.stickY;
                                }
                                publish(SpidyHandCrawlData, [&](hand_crawl::Data& d) {
                                    d.on = cc.on;
                                    d.pull = cc.pull;
                                    d.grip = cc.grip;
                                    d.wallCrawl = gameWallCrawl.load();
                                    d.crawlNow = hold.crawl;
                                    for (unsigned i = 0; i < 2; ++i)
                                        d.grabs[i] += climb.grabbed[i];
                                    d.climbFrames += climb.active;
                                    d.crawlFrames += hold.crawl;
                                    d.swingWallFrames +=
                                        swingState.status &&
                                        swingState.wall == static_cast<uint32_t>(game_swing::Surface::wall);
                                    if (climb.active) {
                                        d.lastStickX = climb.stickX;
                                        d.lastStickY = climb.stickY;
                                        d.peakSpeed = std::max(d.peakSpeed, length(climb.velocity));
                                    }
                                });
                            }
                            // On the game's own crawl, with the view turned
                            // onto the wall, what the game makes of that stick
                            // is the one thing no session has measured.
                            if (hold.crawl && motion.standing) {
                                ++crawlStood;
                                crawlStick.update(hold.normal, motion.walkForwardAxis, walk.right, walk.forward,
                                                  swingState.velocity);
                            }
                            // Y is the game's Y: interact, or web strike in a fight.
                            // B is its B: the dodge, the way the stick points. Not
                            // while a web carries the player, nor while B skips the
                            // calibration.
                            walk.dodge = (origins.play() & buttonB) && !calibrationInput;
                        }
                        // What each hand points at among the game's interactions near
                        // the player, on Spidy's wall only: off it the game offers its
                        // own, and its interact uses them. Read every frame all the
                        // same: a use goes on once the game's crawl has him.
                        const bool onOwnWall =
                            swingState.status && swingState.wall == static_cast<uint32_t>(game_swing::Surface::wall);
                        const bool offersRead = sampleOffers && !sampleOffers(&offers) && offers.status == 1;
                        std::array<offer_pointing::Offer, game_offers::maxOffers> candidates{};
                        unsigned candidateCount{};
                        if (offersRead && onOwnWall)
                            for (uint32_t k = 0; k < offers.offers && k < game_offers::maxOffers; ++k) {
                                const auto& o = offers.nearby[k];
                                candidates[candidateCount++] = offer_pointing::fromGame(
                                    o.handle, o.position, o.reasons, o.enabled != 0, o.reachable != 0);
                            }
                        offersSeen = std::max(offersSeen, candidateCount);
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& hand = motion.swing.hands[i];
                            pointing[i] = std::nullopt;
                            if (steering && !calibrationInput && candidateCount)
                                pointing[i] = offer_pointing::pick(
                                    {hand.tracked, hand.aim.position, hand.aim.orientation.rotate({0, 0, -1})},
                                    candidates.data(), candidateCount);
                            // A tick in the hand as it comes onto one it can use.
                            const uint32_t usable = pointing[i] && pointing[i]->usable ? pointing[i]->handle : 0;
                            if (usable && usable != pointedBefore[i])
                                runtime.haptic(static_cast<int>(i), .25f);
                            pointedBefore[i] = usable;
                        }
                        const auto pointed = offer_pointing::either(pointing[0], pointing[1]);
                        // The game tries the wall round the one pointed at for a
                        // spot within reach (game_offers::aim).
                        if (uint32_t aim = pointed ? pointed->handle : 0; aimOffer && aim != offerAimed) {
                            offerAimed = aim;
                            aimOffer(&aim);
                        }
                        offerPointFrames += pointed && pointed->usable;
                        offerReachFrames += pointed && !pointed->usable;
                        // Y reaches for a thing wherever the player stands, a wall
                        // included; the step up to it below stays on the ground. On
                        // Spidy's wall at one of the game's interactions a hand points
                        // at, it is that one's use: the game's crawl under the player
                        // there, then the game's interact for him.
                        // A Y the game names goes out as it is (NamedPress), with no
                        // turn or step of the interact's.
                        const uint32_t named = namedPress.claimed();
                        walk.interact = offerUse.update(
                            frame.seconds,
                            (reaching || inFlight) && (origins.play() & buttonY) != 0 &&
                                !(named & game_pad::NamedPress::controlY),
                            onOwnWall, pointed,
                            {offers.usesTaken, offers.usesDeclined, offers.best},
                            [&](uint32_t handle) { return useOffer && !useOffer(&handle); });
                        if (offerUse.asks() != offerAsksBefore) {
                            offerAsksBefore = offerUse.asks();
                            runtime.haptic(0, .4f);
                            runtime.haptic(1, .4f);
                        }
                        if (offerUse.failures() != offerFailuresBefore) {
                            offerFailuresBefore = offerUse.failures();
                            message(3, 0, "The game would not take that from here - come closer to it and press "
                                          "again");
                        }
                        // A press of Y turns the hero to where the player looks; held
                        // on, he walks there, until the game takes the press. The
                        // player's own stick goes first.
                        const bool fight = moverTable == config.base + bridge::combatCamera;
                        const float stepIn = interactStep.update(
                            frame.seconds, walk.interact,
                            playing && !mounting && swingState.grounded && !fight && !motion.standing,
                            std::hypot(swingState.velocity.x, swingState.velocity.z));
                        if (interactStep.steps() != interactStepsBefore) {
                            interactStepsBefore = interactStep.steps();
                            runtime.haptic(0, .2f);
                        }
                        // The game's Y: the player's, and after a tap the time the
                        // hero takes to turn to what they look at.
                        walk.interact = interactStep.pressed();
                        // What the game names now, for a pull (below). Not in the pad
                        // layer: there the player presses the game's own buttons.
                        const auto asked = layerOn ? prompt_buttons::Demand{} : game_prompts::demand();
                        // The game takes its own interact away the moment the
                        // player leaves the surface, and the swing's wall is
                        // the player off it: hand him back for the press. Only
                        // where the game is offering nothing, which is the one
                        // case the press does nothing at all today.
                        const bool handing = wallHandover.update(
                            frame.seconds, walk.interact,
                            swingState.wall == static_cast<uint32_t>(game_swing::Surface::wall),
                            game_prompts::live().demand != 0);
                        // The walls are NOT let go of yet, and October 13 is why:
                        // they were, and the player fell. Leaving the wall takes
                        // the swing's hold off a hero the game still has in its
                        // air state, and the game's own crawl does not pick him
                        // up where he stands - it takes him when a stick pushes
                        // him into a wall, which is the half this does not do.
                        // Until that half is built and measured, the rule only
                        // counts: `wall_handovers` says how often a player asked
                        // to interact on a wall of the swing's with the game
                        // offering nothing, which is the size of the thing to
                        // fix. At a vent today the answer is wall running off.
                        wallsHandedBack = handing;
                        (void)swingSettingsSent;
                        (void)toldHandoverRefused;
                        // Not on a wall the player stands on: the game's camera
                        // there is not the follow camera's level turn.
                        walk.look = cameraLook.update(motion.headPose.orientation.rotate({0, 0, -1}),
                                                      {camera[8], camera[9], camera[10]},
                                                      steering && !motion.onSurface);
                        walk.lookUp = cameraPitch.update(motion.headPose.orientation.rotate({0, 0, -1}),
                                                         {camera[8], camera[9], camera[10]},
                                                         steering && !motion.onSurface);
                        if (stepIn > 0 && std::hypot(walk.right, walk.forward) < .1f) {
                            Vec3 look = motion.headPose.orientation.rotate({0, 0, -1});
                            Vec3 ahead{camera[8], 0, camera[10]};
                            look.y = 0;
                            // Looking straight down or up says no way to go.
                            if (length(look) > .3f && length(ahead) > .3f) {
                                look = normalized(look);
                                ahead = normalized(ahead);
                                walk.forward = stepIn * dot(look, ahead);
                                walk.right = stepIn * dot(look, cross(ahead, {0, 1, 0}));
                            }
                        }
                        // While a use is under way the game's crawl is where the
                        // offer was checked: the stick does not walk him off it.
                        if (offerUse.busy())
                            walk.right = walk.forward = 0;
                        // The game's LB + RB, its web pull where it shows the two
                        // (a door, a grate): both web buttons on the thing, which
                        // no web holds on, and a yank; or the right stick's click.
                        std::array<WebPullHand, 2> pulling{};
                        bool free = swingStarted && swingState.grounded && !calibrationInput;
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& hand = motion.swing.hands[i];
                            const auto& aim = aimed.hands[i];
                            free = free && !swingState.webs[i].attached && !grabPhases[i];
                            pulling[i] = {hand.tracked && hand.grip > .5f,
                                          aim.kind == static_cast<uint32_t>(game_swing::AimKind::blocked), aim.point,
                                          hand.aim.position, hand.gripRelativeToHead};
                        }
                        const bool yanked =
                            webPull.update(frame.seconds, playing && free, trackingTurn(motion.swing), pulling);
                        if (webPull.pulls() != pullsBefore) {
                            pullsBefore = webPull.pulls();
                            runtime.haptic(0, 1);
                            runtime.haptic(1, 1);
                            feel(body_haptics::Kind::webPull);
                            message(3, 0, "Web pull - the game's LB + RB, for as long as you hold both web buttons");
                        }
                        const bool pullClick =
                            frame.hands[1].stickClick && !(named & game_pad::NamedPress::rightClick) &&
                            !flyNow.flying && !flyChord.claims(); // Update 38.5: while flying R3 is FLY's
                        const bool pulled =
                            pullPress.update(frame.seconds, reaching, pullClick, frame.hands[0].stickClick) ||
                            (yanked && playing);
                        // A yank at a thing the game is prompting both triggers for
                        // sends those (its LT + RT for a vent as readily as its
                        // LB + RB for a door): the gesture is the player's first way
                        // in. With nothing of the kind named it is the pull it was.
                        if (pulled && asked.leftTrigger && asked.rightTrigger)
                            walk.demand = {0, true, true};
                        else
                            walk.pull = pulled;
                        if (walk.pull && !pulling_)
                            game_prompts::note(onWall ? "the player's pull -> the game's LB + RB, on a wall"
                                                      : "the player's pull -> the game's LB + RB");
                        pulling_ = walk.pull;
                        // The right stick pushed down heals, in the air as on
                        // the ground; pushed up it is the game's LT + RT, while
                        // a web carries the player too (inFlight). Left and
                        // right turn.
                        // The stick as it is, whether or not the controller's pose is tracked: a pose
                        // lost for a moment (the controller out of the cameras' sight, as it is held
                        // down by the hip) took the stick to rest and spent a heal under the thumb.
                        const auto& turning = frame.hands[1];
                        // Not a push the game names a D-pad way for (NamedPress).
                        const bool sticking =
                            steering && !calibrationInput && !layerOn &&
                            !(named & (game_pad::NamedPress::stickUp | game_pad::NamedPress::stickLeft |
                                       game_pad::NamedPress::stickRight));
                        // Update 37: heal_hold_ms, how long a push is held before it counts.
                        stickPush.hold(static_cast<float>(healHoldMs.load()) / 1000.f);
                        const auto push =
                            stickPush.update(frame.seconds, sticking, turning.stickX, turning.stickY);
                        // Update 37: heal 0 leaves the push down unused.
                        walk.heal = healOn.load() && push == game_pad::StickPush::Way::down;
                        // Held on into the game's aim mode (its own time slowed), LT stays down alone,
                        // and letting go zips to the point its aim holds (ZipPress).
                        const bool zipUp = (reaching || inFlight) && push == game_pad::StickPush::Way::up;
                        const auto gameTime = game_time::telemetry();
                        const bool slow = gameTime.installed && gameTime.own < game_pad::ZipPress::slowBelow;
                        const auto zipKeys = zipPress.update(frame.seconds, zipUp, sticking, slow);
                        walk.zip = zipKeys.zip;
                        walk.aim = zipKeys.aim;
                        if ((walk.heal && !healing) || (walk.zip && !zipping))
                            runtime.haptic(1, .2f);
                        if (walk.heal && !healing) {
                            feel(body_haptics::Kind::heal);
                            u37.healPressed = true;
                            game_prompts::note("Update 37: right stick down -> the game's heal");
                        }
                        if (walk.zip && !zipping)
                            feel(body_haptics::Kind::zip);
                        // The swing hears of it with the next command: the zip from its own wall
                        // too, where Y is the wall's interactions' (offerUse).
                        // And of any button the game names, pressed in flight: the
                        // dive it asks for, its web swing, its Ground Strike.
                        askingGame = steering && swingState.owned && !calibrationInput &&
                                     (walk.zip ||
                                      (inFlight && (walk.interact || walk.demand.any() || named != 0)));
                        walk.jump = (keys & swingJumpKey) != 0;
                        // A jump on the game's own crawl - the player's, a web's
                        // takeoff, or the mount's that walks him on from it onto
                        // Spidy's wall - goes out as its RT + A, the only one
                        // that leaves the wall; A alone leaps along it. Only
                        // while the crawl holds him: RT held on in the air is
                        // the game's own web swing.
                        //
                        // From the moment a web holds, too, so the RT is down
                        // before the takeoff's A: the same RT + A pressed at
                        // once just after a web took hold was ignored the first
                        // time, and only the takeoff's retry 0.6 s later left
                        // the wall (October 14). RT alone on the crawl does
                        // nothing, so holding it early costs nothing.
                        //
                        // No longer up the wall with the stick: that was the
                        // game's own wall run (October 14), and since October 15
                        // the walls are Spidy's alone - the stick on the game's
                        // crawl walks the player on, onto them.
                        {
                            const bool onCrawl = swingState.status &&
                                                 swingState.wall == static_cast<uint32_t>(game_swing::Surface::game);
                            const bool webbed = swingState.webs[0].attached || swingState.webs[1].attached;
                            walk.parkour = onCrawl && (walk.jump || webbed);
                        }
                        walk.scan = templePress.held() && steering;
                        // A perfect dodge: the game slows its own time within a
                        // moment of a B that went down with it at full speed.
                        if (walk.dodge && !dodging) {
                            dodgeWatch = game_time::telemetry().own > .95f ? .7f : 0.f;
                            // A whoosh across the body the way the stick sends him.
                            dodgeFrom = walk.right > 0 ? -1.f : 1.f;
                            feel(body_haptics::Kind::dodge, -1, 1, dodgeFrom);
                        }
                        // A dodge while a warning stands: the answer a red mark asks for.
                        if (walk.dodge && !dodging &&
                            std::any_of(senseMarks.begin(), senseMarks.end(),
                                        [](const SenseMark& m) { return m.live && !m.brokenAt; }))
                            ++senseDodges;
                        if (dodgeWatch > 0) {
                            dodgeWatch -= std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : .1f;
                            if (const auto time = game_time::telemetry(); time.installed && time.own < .9f) {
                                dodgeWatch = 0;
                                moments.start(Moment::dodge);
                                runtime.haptic(0, .5f);
                                runtime.haptic(1, .5f);
                                feel(body_haptics::Kind::perfectDodge, -1, 1, dodgeFrom);
                            }
                        }
                        auto buttons = origins.filtered(frame, mapping);
                        // An X the game names is its X: no wheel, no menu (NamedPress).
                        if (named & game_pad::NamedPress::controlX)
                            buttons.buttons &= ~static_cast<uint32_t>(buttonX);
                        // Version 0.0.72 PUNISHER VAN (EXPERIMENTAL, van072.hpp): the PUNISHER's X tap summons it
                        // (his X is free: no gadget wheel; an X the game names stays the game's, above). The van
                        // needs Models\Vehicle\PunisherVan (the Vandura, owner pt850u): missing, it self-disables
                        // with MODEL MISSING on the wrist (no stand-in, nothing spawned, X still taken off the game).
                        // Loaded, the tap asks the ray batch for a spot (vanWanted; the probe, above).
                        if (c38(combat38::punisherVan) == 1 && heroNow() == hero_modes::punisher &&
                            mapping == game_pad::Mapping::gameplay) {
                            const bool vanX = (buttons.buttons & buttonX) != 0;
                            buttons.buttons &= ~static_cast<uint32_t>(buttonX);
                            if (vanX && !vanXWas) {
                                if (mesh_model::library().status[mesh_model::slotVan] == mesh_model::loaded) {
                                    if (van.state != van072::driving && !vanProbe.active) {
                                        vanWanted = true;
                                        runtime.haptic(0, .1f);
                                    }
                                } else {
                                    runtime.haptic(0, .15f);
                                    message(3, 0, "PUNISHER VAN (EXPERIMENTAL): MODEL MISSING: Vehicle/PunisherVan, no van");
                                    game_prompts::note("Version 0.0.72 PUNISHER VAN: X tap - MODEL MISSING (Models\\Vehicle\\PunisherVan), no van spawned");
                                }
                            }
                            vanXWas = vanX;
                        } else
                            vanXWas = false;
                        // The gadget wheel: X held opens it in front of the hand
                        // X is on (the left on Touch and Index, the right on the
                        // Steam Frame); X tapped is the game menu as it is let go.
                        // Chosen, a gadget is equipped in the game and the free
                        // hand's trigger fires it; the web shooter at the top
                        // has it fire web balls again.
                        {
                            const unsigned hand = runtime.controllers()[1] == ControllerKind::frame ? 1u : 0u;
                            const uint32_t owned = std::min(listed.gadgetCount, 8u);
                            uint8_t places = 1;
                            for (uint32_t g = 0; g < owned; ++g)
                                if (listed.gadgets[g].place < gadgetPlaces)
                                    places |= static_cast<uint8_t>(1u << listed.gadgets[g].place);
                            const bool offered = wheelShooter && shooterStarted && !listed.gadgetError && owned &&
                                                 mapping == game_pad::Mapping::gameplay &&
                                                 hero_modes::webPaths(heroNow()).gadgetWheel; // 0.0.4: no web gadgets
                            const bool x = (buttons.buttons & buttonX) != 0;
                            if (offered)
                                buttons.buttons &= ~static_cast<uint32_t>(buttonX);
                            const auto chosen =
                                gadgetWheel.update(frame.seconds, x, offered && steering && !calibrationInput,
                                                   frame.hands[hand].grip.position, frame.head.position, places);
                            if (chosen.tap)
                                menuTap = .1f;
                            if (chosen.opened) {
                                runtime.haptic(hand, .3f);
                                feel(body_haptics::Kind::gadget, static_cast<int>(hand), .3f);
                            }
                            if (chosen.lit) {
                                runtime.haptic(hand, .12f);
                                feel(body_haptics::Kind::gadget, static_cast<int>(hand), .15f);
                            }
                            if (chosen.chosen >= 0) {
                                runtime.haptic(hand, .5f);
                                feel(body_haptics::Kind::gadget, static_cast<int>(hand), .5f);
                                game_shooter::GadgetCommand command;
                                for (uint32_t g = 0; g < owned; ++g)
                                    if (listed.gadgets[g].place == chosen.chosen)
                                        command.equip = listed.gadgets[g].weapon;
                                gadgetFire = command.equip != 0;
                                command.fire = gadgetFire ? 1u : 0u;
                                wheelShooter(&command);
                            }
                        }
                        auto pad = game_pad::fromControllers(buttons, mapping, walk);
                        // What the controls of the names the game shows send it.
                        if (mapping == game_pad::Mapping::gameplay) {
                            pad.buttons |= namedPad.buttons;
                            pad.leftTrigger = std::max(pad.leftTrigger, namedPad.leftTrigger);
                            pad.rightTrigger = std::max(pad.rightTrigger, namedPad.rightTrigger);
                        }
                        if (flyNow.flying && mapping == game_pad::Mapping::gameplay)
                            pad.buttons = static_cast<uint16_t>(pad.buttons & ~game_pad::y); // FLY replaces Web Wings
                        if (vanDriving && mapping == game_pad::Mapping::gameplay) { // Version 0.0.72: the van's pedals
                            pad.leftTrigger = {};
                            pad.rightTrigger = {};
                        }
                        if (mapping == game_pad::Mapping::gameplay) { // Update 38.5: FLY's R3 / L3+R3
                            if (flyNow.flying && (flyChord.claims() || frame.hands[1].stickClick))
                                pad.buttons = static_cast<uint16_t>(pad.buttons & ~(game_pad::leftThumb | game_pad::rightThumb));
                            if (flyChord.tapping())
                                pad.buttons = static_cast<uint16_t>(pad.buttons | game_pad::leftThumb | game_pad::rightThumb);
                        }
                        if (namedPress.presses() != namedPressesBefore) {
                            namedPressesBefore = namedPress.presses();
                            char line[96];
                            sprintf_s(line, "the player's press of a button the game names -> 0x%04x%s%s%s",
                                      namedPad.buttons, namedPad.leftTrigger ? " LT" : "",
                                      namedPad.rightTrigger ? " RT" : "",
                                      named & game_pad::NamedPress::fist ? ", a thrown fist" : "");
                            game_prompts::note(line);
                        }
                        if (menuTap > 0) {
                            pad.buttons |= game_pad::back;
                            menuTap -= std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : .1f;
                        }
                        screenControls.apply(frame.seconds,
                                             flatScreen && motion.active && mapping == game_pad::Mapping::menus,
                                             buttons, pad);
                        // Both thumbsticks held in pause the game in every mapping, the menu button as
                        // before: under SteamVR a Quest's menu button opens SteamVR's dashboard instead.
                        if (pausePress.update(frame.seconds, mapping != game_pad::Mapping::none,
                                              frame.hands[0].stickClick, frame.hands[1].stickClick))
                            pad.buttons |= game_pad::start;
                        if (pausePress.spent())
                            pad.buttons &=
                                static_cast<uint16_t>(~(game_pad::leftThumb | game_pad::rightThumb));
                        if (pausePress.pauses() != pausesBefore) {
                            pausesBefore = pausePress.pauses();
                            runtime.haptic(0, .4f);
                            runtime.haptic(1, .4f);
                        }
                        // Update 37: the web shooter and the gadgets by gesture, and hand
                        // hits, as the game's own pad buttons (combat_input.hpp).
                        {
                            combat_input::Frame cf;
                            cf.seconds = frame.seconds;
                            // Update 38: in the air and on a swing too (run372: 0 punches sent in flight).
                            cf.play = (playing || inFlight) && !calibrationInput && !layerOn && !phone.open() &&
                                      !gadgetWheel.open() && !padLayer.latched();
                            cf.airborne = inFlight;
                            // Version 0.0.62: FLY MODE in the air - the triggers thrust, so a held one holds no punch back.
                            // IRON MAN keeps his trigger (the repulsor fires in flight, as in 0.0.6).
                            cf.flightTriggers = flyModeNow.load() != 0 && flyNow.flying && heroNow() != hero_modes::ironMan;
                            cf.airborne = cf.airborne || (flyModeNow.load() != 0 && flyNow.flying);
                            for (unsigned i = 0; i < 2; ++i) {
                                auto& h = cf.hands[i];
                                const auto& grip = frame.hands[i].grip;
                                h.tracked = gripsTracked[i];
                                h.free = !swingState.webs[i].attached && !grabPhases[i] && !u37.taken[i] &&
                                         !u37.climbing[i];
                                // Update 38: SpidyVR's busy rule - a hand whose own web holds or
                                // carries, a grab, a taken web or a climbing hand punches nothing.
                                h.busy = !h.free;
                                h.button = u37.ball[i];
                                h.grip = u37.other[i];
                                // Version 0.0.72 PUNISHER VAN: driving, the trigger is the van's pedal and nothing else.
                                vanTrigger[i] = h.button;
                                if (vanDriving)
                                    h.button = 0;
                                h.position = grip.position;
                                // Update 38.1 AIM AXIS: HAND (webShooter(grip) -Z, the swing webs'), AIM
                                // POSE or 38's grip -Z (DIAGNOSIS §0); the glob, assist, contact fist,
                                // yank, low aim, flick and blood all take this ray.
                                h.aim = combat38::aimOf(grip, frame.hands[i].aim, c38(combat38::aimAxis));
                                h.palm = grip.orientation.rotate({0, -1, 0});
                                // Update 38 ZIP MODE AIMED: the wrist's web ray on a character.
                                h.onCharacter = aimed.hands[i].kind == static_cast<uint32_t>(game_swing::AimKind::character);
                                // Update 38.2: the pointing hand in the hologram's zone attacks nothing.
                                h.zoned = (hudZone & (1u << i)) != 0 ||
                                          (i != (wristHudHand.load() ? 1u : 0u) && wristToggle.diagnostics().inZone);
                            }
                            // Update 38.2 TARGETED: no attack with the hologram open. Update 38.5: the
                            // mirror no longer blocks punches (run384: a fight in front of it did nothing).
                            cf.menuOpen = wristToggle.opacity() > 0;
                            {   // Update 38: the enemies near (WORLD LOG's 10 Hz list), into tracking space.
                                std::array<Vec3, world_trace::enemySlots> nearby{};
                                std::array<uint64_t, world_trace::enemySlots> nearbyIds{};
                                uint32_t n =
                                    std::min<uint32_t>(world_trace::enemies(nearby, &nearbyIds), world_trace::enemySlots);
                                // Version 0.0.62 ENEMY TRACKING LIVE (the SpidyVR comparison: its targets are
                                // read live each sample): each enemy at his own world position, the list 30
                                // times a second. 0.0.6 put him at the player's feet NOW plus his offset from
                                // the player up to 0.1 s ago, so while the player moved (a lunge at 8 m/s:
                                // 0.8 m) every enemy trailed behind - the lock, the punch contact and the
                                // TARGETED reach all aimed at where he was not. The world list is used only
                                // while the hero it was measured from is near the player's feet (the same
                                // matrix; a mismatch falls back to 0.0.6's offsets for that frame).
                                const bool liveFoes = combat38::foeTrackLive(c38(combat38::foeTrack));
                                world_trace::setFastRefresh(liveFoes);
                                bool foesWorld = false;
                                if (liveFoes && n) {
                                    std::array<Vec3, world_trace::enemySlots> worldFeet{};
                                    std::array<uint64_t, world_trace::enemySlots> worldIds{};
                                    float age = 1e9f;
                                    Vec3 heroThen{};
                                    const uint32_t wn = std::min<uint32_t>(world_trace::enemiesWorld(worldFeet, worldIds, age),
                                                                           world_trace::enemySlots);
                                    if (wn && world_trace::heroAtRefresh(heroThen) && finite(motion.anchor) &&
                                        combat38::foeWorldUsable(length(heroThen - motion.anchor), age,
                                                                 finite(swingState.velocity) ? length(swingState.velocity) : 0.f)) {
                                        for (uint32_t e = 0; e < wn; ++e)
                                            nearby[e] = worldFeet[e] - motion.anchor; // the same sum below, now exact
                                        nearbyIds = worldIds;
                                        n = wn;
                                        foesWorld = true;
                                    }
                                }
                                foesLiveFrames += foesWorld ? 1u : 0u;
                                foesOldFrames += !foesWorld && n ? 1u : 0u;
                                const Pose fromWorld = inverse(compose(motion.headPose, inverse(frame.head)));
                                for (uint32_t e = 0; e < n && cf.enemyCount < combat38::maxEnemies; ++e) {
                                    const Vec3 enemyFeet = compose(fromWorld, Pose{motion.anchor + nearby[e], {}}).position;
                                    if (finite(enemyFeet)) {
                                        cf.enemyIds[cf.enemyCount] = nearbyIds[e];
                                        cf.enemySight[cf.enemyCount] = sightOf(nearbyIds[e]);
                                        cf.enemies[cf.enemyCount++] = enemyFeet;
                                    }
                                }
                                // The player's feet (the mover's anchor), tracking space.
                                cf.feet = compose(fromWorld, Pose{motion.anchor, {}}).position;
                                cf.hasFeet = finite(cf.feet) && finite(motion.anchor);
                            }
                            {   // Update 38 LUNGE SPIDYVR MOVE: the bridge's lunges; the X on a landing.
                                const bool wantsMove = c38(combat38::lunge) == combat38::lungeSpidyMove ||
                                                       flyModeNow.load() != 0; // Update 38.5 FLY's lunge
                                cf.bridgeLunge = wantsMove && sampleLunge && swingStarted;
                                if (sampleLunge && !sampleLunge(&lungeState) && lungeState.magic == 0x4c4e4753 &&
                                    lungeState.version == 1 && lungeState.bytes == sizeof(lungeState)) {
                                    if (!lungeBaseSet) {
                                        lungeOpenedBase = lungeState.opened;
                                        lungeMissedBase = lungeState.missed;
                                        lungeLandedWas = lungeState.landed;
                                        lungeBaseSet = true;
                                    }
                                    cf.lungeLanded = lungeState.landed > lungeLandedWas;
                                    lungeLandedWas = lungeState.landed;
                                } else {
                                    lungeState = {};
                                }
                                if (wantsMove && !sampleLunge && !lungeFallbackNoted) {
                                    lungeFallbackNoted = true;
                                    game_prompts::note("Update 38: LUNGE SPIDYVR MOVE needs Update 38's ray module "
                                                       "(SpidyLungeSample); the lunge stays GAME BUTTON");
                                }
                            }
                            cf.head = frame.head.position;
                            cf.forward = frame.head.orientation.rotate({0, 0, -1});
                            // Update 37.2: no gadget with a web attached; the zip strike's distance.
                            cf.webAttached = swingState.webs[0].attached || swingState.webs[1].attached;
                            cf.enemyMetres = world_trace::nearestEnemyMetres();
                            const auto config37 = combatConfig();
                            // The enemies for the next frame's grab press (the swing input runs first).
                            grabEnemies = cf.enemies;
                            grabEnemyIds = cf.enemyIds;
                            grabEnemyCount = cf.enemyCount;
                            grabAirborne = cf.airborne;
                            // Update 38.2 GRAB WEB: each held grab follows its enemy (by id); the hand is
                            // busy (no punch, no gadget from it); a pull toward the body yanks once.
                            for (unsigned i = 0; i < 2; ++i) {
                                auto& grab = grabWebs[i];
                                if (!grab.on) {
                                    grabHandSet[i] = false;
                                    continue;
                                }
                                if (ropes[i].on)
                                    continue; // Version 0.0.3 ROPE GRAB holds this rope (below)
                                int found = -1;
                                for (uint32_t e = 0; e < cf.enemyCount; ++e)
                                    if (cf.enemyIds[e] == grabIds[i]) {
                                        found = static_cast<int>(e);
                                        break;
                                    }
                                if (found >= 0) {
                                    grab.enemy = found;
                                    grab.at = cf.enemies[static_cast<size_t>(found)] + Vec3{0, combat38::centreMass, 0};
                                }
                                const float dt = std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                                const Vec3 rel = cf.hands[i].position - cf.head;
                                Vec3 velocity{};
                                if (grabHandSet[i] && dt > 1e-4f)
                                    velocity = (rel - grabHandWas[i]) / dt;
                                grabHandWas[i] = rel;
                                grabHandSet[i] = finite(rel);
                                const Vec3 toBody = cf.head + Vec3{0, -.35f, 0} - cf.hands[i].position;
                                if (grab.update(dt, grabSqueeze[i] > .5f && cf.hands[i].tracked, found >= 0, velocity, toBody)) {
                                    // Update 38.5: the pull is a yank, so it sends the yank's input (YANK SLOT,
                                    // default Y HOLD: the game's published hold Triangle, CANDIDATE); YANK SLOT
                                    // OFF keeps 38.4's Grabber input (run384: 5 grab yanks on it, none felt).
                                    if (config37.yankSlot) {
                                        combatMachine.sendGadget(combat_input::webYank, config37.yankSlot);
                                        ++grabYankInputs;
                                        if (grabAirborne)
                                            ++yanksInAir;
                                        char line[128];
                                        sprintf_s(line, "Update 38.5: GRAB WEB yank -> the game's %s (WEB YANK input, CANDIDATE)%s",
                                                  combat_input::slotName(config37.yankSlot),
                                                  grabAirborne ? ", in the air" : "");
                                        game_prompts::note(line);
                                    } else {
                                        combatMachine.sendGadget(combat_input::webGrabber, config37.grabberSlot);
                                        game_prompts::note("Update 38.2: GRAB WEB yank -> the game's Grabber input (CANDIDATE)");
                                    }
                                    runtime.haptic(static_cast<int>(i), .9f);
                                    if (c38(combat38::gadgetHint) == 0) { // Update 38.3 GADGET HINT
                                        gadgetHints[i].show(combat38::hintYank);
                                        ++gadgetHintsShown;
                                    }
                                }
                                if (grab.on) {
                                    cf.hands[i].free = false;
                                    cf.hands[i].busy = true;
                                    grabFrom[i] = validTrackedPose(frame.hands[i].grip)
                                                      ? spidy::webShooter(frame.hands[i].grip, i).position
                                                      : cf.hands[i].position;
                                    grabTo[i] = grab.at;
                                }
                            }
                            // Version 0.0.3 ROPE GRAB: each tether follows its enemy (by id) and its hand; a
                            // fling, a slam, both hands smashed together or a fast let-go releases him and sends
                            // the game's own yank (the same input as GRAB WEB's, CANDIDATE effect); WALL STICK
                            // is armed by it. Nothing writes the enemy (no SM2 enemy write is confirmed).
                            bool wallArmNow = false;
                            {
                                const float dt = std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                                for (unsigned i = 0; i < 2; ++i) {
                                    const Vec3 rel = cf.hands[i].position - cf.head;
                                    ropeHandVelocity[i] = ropeHandSet[i] && dt > 1e-4f ? (rel - ropeHandWas[i]) / dt : Vec3{};
                                    ropeHandWas[i] = rel;
                                    ropeHandSet[i] = finite(rel);
                                }
                                bool clap = ropes[0].on && ropes[1].on && cf.hands[0].tracked && cf.hands[1].tracked &&
                                            rope_grab::clapped(cf.hands[0].position, cf.hands[1].position,
                                                               ropeHandVelocity[0], ropeHandVelocity[1]);
                                if (clap)
                                    ++ropeClaps;
                                for (unsigned i = 0; i < 2; ++i) {
                                    auto& rope = ropes[i];
                                    if (!rope.on)
                                        continue;
                                    int found = -1;
                                    for (uint32_t e = 0; e < cf.enemyCount; ++e)
                                        if (static_cast<uint32_t>(cf.enemyIds[e]) == rope.id) {
                                            found = static_cast<int>(e);
                                            break;
                                        }
                                    uint32_t why = rope_grab::none;
                                    if (clap) {
                                        why = rope.stop(rope_grab::clap);
                                    } else {
                                        const Vec3 aim = validTrackedPose(frame.hands[i].grip)
                                                             ? combat38::aimOf(frame.hands[i].grip, frame.hands[i].aim,
                                                                               c38(combat38::aimAxis))
                                                             : Vec3{};
                                        const Vec3 hand = validTrackedPose(frame.hands[i].grip)
                                                              ? spidy::webShooter(frame.hands[i].grip, i).position
                                                              : cf.hands[i].position;
                                        why = rope.update(dt, grabSqueeze[i] > .5f && cf.hands[i].tracked, found >= 0,
                                                          hand, aim);
                                    }
                                    if (found >= 0) {
                                        grabWebs[i].enemy = found;
                                        grabWebs[i].at = cf.enemies[static_cast<size_t>(found)] + Vec3{0, combat38::centreMass, 0};
                                    }
                                    if (why != rope_grab::none) {
                                        grabWebs[i].release();
                                        char line[128];
                                        if (rope_grab::yanks(why, rope.releaseVelocity)) {
                                            if (config37.yankSlot)
                                                combatMachine.sendGadget(combat_input::webYank, config37.yankSlot);
                                            else
                                                combatMachine.sendGadget(combat_input::webGrabber, config37.grabberSlot);
                                            ++ropeYanksSent;
                                            ++grabYankInputs;
                                            wallArmNow = true;
                                            runtime.haptic(static_cast<int>(i), .9f);
                                            sprintf_s(line, "Version 0.0.3: ROPE GRAB %s at %.1f m/s -> the game's yank (CANDIDATE)",
                                                      rope_grab::releaseNames[why % rope_grab::releases],
                                                      length(rope.releaseVelocity));
                                        } else {
                                            sprintf_s(line, "Version 0.0.3: ROPE GRAB %s",
                                                      rope_grab::releaseNames[why % rope_grab::releases]);
                                        }
                                        game_prompts::note(line);
                                        continue;
                                    }
                                    cf.hands[i].free = false;
                                    cf.hands[i].busy = true;
                                    grabFrom[i] = validTrackedPose(frame.hands[i].grip)
                                                      ? spidy::webShooter(frame.hands[i].grip, i).position
                                                      : cf.hands[i].position;
                                    grabTo[i] = grabWebs[i].at; // the real enemy: the rope ends on him, rigid
                                }
                                // The BOT STATE PROBE watches the first held enemy (read-only).
                                const uint64_t watch = ropes[0].on ? grabIds[0] : ropes[1].on ? grabIds[1] : 0;
                                if (watch != ropeWatched) {
                                    world_trace::watchBot(watch != 0, watch);
                                    ropeWatched = watch;
                                }
                            }
                            const auto fightOut = combatMachine.update(cf, config37);
                            if (const auto& eh = combatMachine.everyHit();
                                eh.taps() != everyHitNoted && combatClock - everyHitNotedAt >= 5) {
                                everyHitNoted = eh.taps();
                                everyHitNotedAt = combatClock;
                                char line[200];
                                sprintf_s(line,
                                          "Version 0.0.4 EVERY HIT: %llu X taps (one per blow), mean wait %.0f ms, "
                                          "worst %.0f ms, %llu over the queue",
                                          static_cast<unsigned long long>(eh.taps()),
                                          static_cast<double>(eh.meanDelay()) * 1000.0,
                                          static_cast<double>(eh.worstDelay()) * 1000.0,
                                          static_cast<unsigned long long>(eh.merged()));
                                game_prompts::note(line);
                            }
                            // Version 0.0.3 WALL STICK (EXPERIMENTAL, OFF by default): a punch or a rope release
                            // arms it; an enemy then flying >= 7 m/s and stopping dead gets a web net there.
                            {
                                const uint32_t wallMode = c38(combat38::wallStick);
                                const float dt = std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                                wallClock += dt;
                                if (wallMode != wall_stick::off) {
                                    if (wallArmNow || fightOut.punch[0] || fightOut.punch[1]) {
                                        wallStick.arm();
                                        ++wallArms;
                                    }
                                    float age = -1;
                                    const uint32_t n = world_trace::enemiesWorld(wallFeet, wallIds, age);
                                    if (std::isfinite(age) && age >= 0 && (wallAgeWas < 0 || age < wallAgeWas))
                                        for (uint32_t e = 0; e < n && e < world_trace::enemySlots; ++e)
                                            wallStick.sample(wallIds[e], wallFeet[e], wallClock - age, wallMode);
                                    wallAgeWas = std::isfinite(age) ? age : -1.f;
                                }
                                wallStick.frame(dt, wallMode);
                            }
                            {   // Update 38.5 BALL LOOK WRIST / GLOB: the wrists (world), and the ball
                                // whose RB just went down: its muzzle, its wrist (GAME ONLY: none).
                                // Version 0.0.4: never in REPULSOR (no game web ball, no web look at all).
                                const bool wristBall = combat38::wristMuzzle(c38(combat38::ballLook)) &&
                                                       heroNow() == hero_modes::spiderMan; // 0.0.6: every hero but SPIDER-MAN
                                wrist_muzzle::setEnabled(wristBall);
                                for (unsigned i = 0; i < 2; ++i) {
                                    const bool ok = motion.swing.hands[i].tracked && validTrackedPose(motion.grips[i]);
                                    wrist_muzzle::publish(i, ok ? spidy::webShooter(motion.grips[i], i).position : Vec3{}, ok);
                                    if (fightOut.ballStart[i] && wristBall)
                                        wrist_muzzle::fire(i);
                                }
                            }
                            // Version 0.0.2 BALL LOOK OURS: one ball per press from the pulled hand's wrist along
                            // its aim, bent to the locked or nearest enemy by AIM STRENGTH; a hit buzzes and
                            // splashes (BALL HIT +GAME WEB also asks the game for its ball at the enemy).
                            if (!oursFault) {
                                try {
                                    our_ball::Enemies foes;
                                    for (uint32_t e = 0; e < cf.enemyCount && e < static_cast<uint32_t>(our_ball::maxEnemies); ++e) {
                                        foes.list[foes.count].feet = cf.enemies[e];
                                        foes.list[foes.count++].id = cf.enemyIds[e];
                                    }
                                    oursMotion.update(static_cast<float>(combatClock), foes);
                                    const uint32_t strength = c38(combat38::aimStrength);
                                    // Version 0.0.4 IRON MAN ABILITIES: REPULSOR's plan has no web part at all.
                                    const bool repMode = c38(combat38::shooterType) == repulsor::repulsor;
                                    const repulsor::Plan plan = repulsor::planFor(repMode ? repulsor::repulsor : repulsor::web,
                                                                                  true, c38(combat38::ballHit) == 1);
                                    if (repMode != repWas) {
                                        repWas = repMode;
                                        repTrigger.clear();
                                        oursBalls.clear(); // no web ball drawn as a blast or the other way
                                        game_prompts::note(repMode ? "VERSION 0.0.6 IRON MAN ABILITIES (REPULSOR): palm blasts and the chest beam, "
                                                                     "no webs at all (no swing, zip, web shooter or gadgets); real damage (CONFIRMED path)"
                                                                   : "VERSION 0.0.6 IRON MAN ABILITIES OFF");
                                    }
                                    const auto wristOf = [&](unsigned i) {
                                        const Pose& grip = frame.hands[i].grip;
                                        return validTrackedPose(grip) ? spidy::webShooter(grip, i).position : cf.hands[i].position;
                                    };
                                    // Version 0.0.62 PALM BLAST: Iron Man's hand blasts leave the palm's centre.
                                    const auto palmOf = [&](unsigned i) {
                                        const Pose& grip = frame.hands[i].grip;
                                        return validTrackedPose(grip) ? repulsor::palmCentre(grip, i) : cf.hands[i].position;
                                    };
                                    // Version 0.0.6 DAREDEVIL ABILITIES: the trigger throws that hand's baton at the enemy
                                    // the strong aim assist picks (35 deg, 25 m); a hit sends sm2_damage's batonHit
                                    // (CONFIRMED path), chains up to 2 times (0.0.62: 3 enemies in all) to the nearest other enemy within 5 m
                                    // (never one already hit) and always comes back; no throw while it is out.
                                    if (const uint32_t heroIs = heroNow(); heroIs != heroWas) {
                                        heroWas = heroIs;
                                        for (unsigned i = 0; i < 2; ++i) {
                                            batons[i] = {};
                                            batonSwings[i].reset();
                                            batonThrowsSeen[i] = 0;
                                        }
                                        batonAt = -1;
                                        char line[240];
                                        sprintf_s(line, "VERSION 0.0.6 HERO: %s (%s)", hero_modes::heroNames[heroIs < hero_modes::heroes ? heroIs : 0],
                                                  heroIs == hero_modes::daredevil ? "a baton in each hand, trigger throws, punches x DAREDEVIL PUNCH, sonar look, no webs"
                                                  : heroIs == hero_modes::hulk    ? "4x charged jump, one-punch knockouts, brutes 2-3, no webs"
                                                  : heroIs == hero_modes::ironMan ? "repulsors and the chest beam, no webs"
                                                  : heroIs == hero_modes::batman  ? "0.0.7: hold, let go to throw a batarang; grip at the belt for smoke; no webs"
                                                  : heroIs == hero_modes::wolverine ? "0.0.7: claws on the trigger, healing, no webs"
                                                  : heroIs == hero_modes::superman  ? "0.0.7: both triggers heat vision (hand at mouth: cold breath), super strength, FLY MODE superspeed"
                                                  : heroIs == hero_modes::punisher  ? "0.0.7: grip at a hip or behind the back to draw, trigger fires, EASY RELOAD"
                                                                                  : "webs");
                                        game_prompts::note(line);
                                        // Version 0.0.72 build 6: THOR's enable path on the launcher console, step by step
                                        // (the build 4 / 5 crash was the wrist's ACTIVE HERO name, fixed; these stay).
                                        if (heroIs == hero_modes::thor) {
                                            thorStepsLogged = 0;
                                            consoleLine(std::string("THOR: step 1 hero on (lightning ") +
                                                        (c38(combat38::thorLightning) == 1 ? "off" : "on") + ", storm " +
                                                        (c38(combat38::thorStorm) == 1 ? "off" : "on") + ", chain " +
                                                        (c38(combat38::thorChain) == 1 ? "off" : "on") + ", hammer model " +
                                                        (c38(combat38::thorHammerModel) == 1 ? "off" : "on") + ", flight " +
                                                        (c38(combat38::thorFlight) == 1 ? "off" : "on") + ")");
                                        }
                                    }
                                    if (heroWas == hero_modes::daredevil) {
                                        hero_modes::BatonEnemy batonList[our_ball::maxEnemies];
                                        // Version 0.0.62 BATON BOUNCE: an enemy whose hit points read 0 or less (the
                                        // DAMAGE PROBE's guarded BotHealth reads) is down: never a target. Unread: up.
                                        std::array<world_trace::Vital, world_trace::enemySlots> batonVit{};
                                        const uint32_t batonVn = world_trace::enemyVitals(batonVit);
                                        for (uint32_t e = 0; e < foes.count; ++e) {
                                            batonList[e] = {foes.list[e].id, foes.list[e].feet + Vec3{0, 1.2f, 0}};
                                            for (uint32_t v = 0; v < batonVn && v < batonVit.size(); ++v)
                                                if (batonVit[v].id == foes.list[e].id && batonVit[v].have && batonVit[v].hp <= 0.f)
                                                    batonList[e].down = true;
                                        }
                                        const float batonDt = batonAt < 0 ? 0.f : static_cast<float>(std::clamp(combatClock - batonAt, 0.0, .1));
                                        batonAt = combatClock;
                                        // Version 0.0.7 BATON HOLSTERS ON: a baton throws and swings only once drawn.
                                        const bool ddHolsters = combat38::batonHolstersOn(c38(combat38::batonHolsters));
                                        for (unsigned i = 0; i < 2; ++i) {
                                            auto& b = batons[i];
                                            const bool batonOut = hero007Play.batonDrawn(i, ddHolsters);
                                            const bool trig = cf.hands[i].tracked && cf.hands[i].button > .5f && batonOut;
                                            const Vec3 hand = cf.hands[i].position;
                                            const Vec3 aimDir = cf.hands[i].aim;
                                            batonHand[i] = hand;
                                            batonAimNow[i] = aimDir;
                                            batonTracked[i] = cf.hands[i].tracked;
                                            // Version 0.0.7 BATON GRIP: through the fist from the grip pose, per controller.
                                            {
                                                const auto held = baton_grip::shaft(frame.hands[i].grip, i, runtime.controllers()[i]);
                                                batonShaftOk[i] = held.ok;
                                                batonButt[i] = held.butt;
                                                batonTipHeld[i] = held.tip;
                                            }
                                            const int batonAimed = b.inHandNow() ? hero_modes::batonAim(hand, aimDir, batonList, foes.count) : -1;
                                            const uint32_t throwsWas = b.throws;
                                            const bool hit = finite(hand) && finite(aimDir) &&
                                                             b.update(batonDt, trig, hand, aimDir, batonList, foes.count, batonAimed);
                                            if (b.throws != throwsWas)
                                                runtime.haptic(static_cast<int>(i), .3f);
                                            hero_modes::batonSounds(batonThrowsSeen[i], b, hit);
                                            if (hit && b.hitCount > 0) {
                                                const uint64_t who = b.hitIds[b.hitCount - 1];
                                                const bool sent = who && sm2_damage::queue(who, sm2_damage::batonHit());
                                                batonBlowsSent += sent ? 1 : 0;
                                                runtime.haptic(static_cast<int>(i), .5f);
                                                for (uint32_t e = 0; e < foes.count; ++e)
                                                    if (batonList[e].id == who)
                                                        hitMarks.add(batonList[e].chest);
                                                char line[160];
                                                sprintf_s(line, "VERSION 0.0.6 DAREDEVIL BATON HIT 0x%llx (hit %u of this throw), blow %s",
                                                          static_cast<unsigned long long>(who), b.hitCount,
                                                          sent ? "sent (CONFIRMED path)" : "not sent (damage path off)");
                                                game_prompts::note(line);
                                            }
                                            // Version 0.0.62 BATON SWING: the baton in the hand hits what its shaft
                                            // sweeps through (0.0.6 hurt only with a thrown baton at an aimed enemy).
                                            Vec3 grip{}, tip{};
                                            hero_modes::batonShaft(hand, aimDir, grip, tip);
                                            if (batonShaftOk[i]) { // Version 0.0.7 BATON GRIP: the shaft as it is drawn
                                                grip = batonButt[i];
                                                tip = batonTipHeld[i];
                                            }
                                            const int swung = batonSwings[i].update(batonDt, b.inHandNow() && cf.hands[i].tracked && batonOut,
                                                                                    grip, tip, batonList, foes.count);
                                            if (swung >= 0) {
                                                const uint64_t who = batonList[swung].id;
                                                const bool sent = who && sm2_damage::queue(who, sm2_damage::batonHit());
                                                batonBlowsSent += sent ? 1 : 0;
                                                ++batonSwingHits;
                                                runtime.haptic(static_cast<int>(i), .6f);
                                                sfx::play(sfx::batonHit);
                                                hitMarks.add(batonList[swung].chest);
                                                if (batonSwingHits <= 5 || !sent) {
                                                    char line[160];
                                                    sprintf_s(line, "VERSION 0.0.62 DAREDEVIL BATON SWING HIT 0x%llx (%.1f m/s), blow %s",
                                                              static_cast<unsigned long long>(who),
                                                              static_cast<double>(batonSwings[i].lastSpeed),
                                                              sent ? "sent (CONFIRMED path)" : "not sent (damage path off)");
                                                    game_prompts::note(line);
                                                }
                                            }
                                        }
                                    }
                                    // Version 0.0.72 PUNISHER VAN ENGINE PREVIEW (EXPERIMENTAL, van072.hpp stepPreview): in world
                                    // space (the rays'); the probe's spot, the door handle, the wheel latched to the hands, the
                                    // pedals. Nothing in the world moves (the body carry is NOT WIRED). While he drives the
                                    // triggers are the pedals only: no shot, no hook, nothing to the game (the pad, above).
                                    {
                                        van072::In vin;
                                        vin.dt = vanAt < 0 ? 0.f : static_cast<float>(std::clamp(combatClock - vanAt, 0.0, .1));
                                        vanAt = combatClock;
                                        vin.enabled = c38(combat38::punisherVan) == 1 &&
                                                      mesh_model::library().status[mesh_model::slotVan] == mesh_model::loaded;
                                        vin.punisher = heroWas == hero_modes::punisher;
                                        vin.play = cf.play;
                                        const Pose vanW = compose(motion.headPose, inverse(frame.head));
                                        vin.head = motion.headPose.position;
                                        vin.look = motion.headPose.orientation.rotate({0, 0, -1});
                                        vin.feet = vin.head - Vec3{0, 1.6f, 0};
                                        for (unsigned i = 0; i < 2; ++i) {
                                            vin.hands[i] = compose(vanW, Pose{cf.hands[i].position, {}}).position;
                                            vin.tracked[i] = cf.hands[i].tracked && finite(vin.hands[i]);
                                            vin.grip[i] = cf.hands[i].grip;
                                            vin.trigger[i] = vanTrigger[i];
                                        }
                                        if (vanSpotReady) {
                                            vanSpotReady = false;
                                            vin.summonSpot = true;
                                            vin.spot = vanSpot;
                                        }
                                        const auto flat = [&](float, float, float y) { return y; };   // the preview's street
                                        const auto open = [](Vec3, Vec3, float) { return true; };   // (the probe checked it)
                                        const auto clearAhead = [](Vec3, Vec3, float) { return true; };
                                        const van072::Out vout = van.stepPreview(vin, flat, open, clearAhead);
                                        if (vout.summoned) {
                                            message(1, 0, "PUNISHER VAN (EXPERIMENTAL): here - grip the driver door handle to get in");
                                            game_prompts::note("Version 0.0.72 PUNISHER VAN: summoned (the probe's spot, CANDIDATE)");
                                        } else if (vout.noSpot) {
                                            message(2, 0, "PUNISHER VAN (EXPERIMENTAL): no safe spot here, no van");
                                            game_prompts::note("Version 0.0.72 PUNISHER VAN: no safe spot (the probe found none)");
                                        }
                                        if (vout.entered)
                                            message(1, 0, "PUNISHER VAN: in - both hands on the wheel, RT drive, LT reverse, both brake (PREVIEW)");
                                        if (vout.exited)
                                            message(1, 0, "PUNISHER VAN: out");
                                        for (unsigned i = 0; i < 2; ++i)
                                            if (vout.haptic[i] > 0)
                                                runtime.haptic(static_cast<int>(i), vout.haptic[i]);
                                        vanDriving = van.state == van072::driving;
                                    }
                                    // Version 0.0.7 HERO PROFILES: BATMAN's batarangs, WOLVERINE's claws, SUPERMAN's
                                    // heat vision / cold breath, PUNISHER's guns (hero007_play.hpp). Every blow on
                                    // sm2_damage's path (CONFIRMED), as DAREDEVIL's batons; the rest is visual.
                                    if (heroWas >= hero_modes::batman && heroWas < hero_modes::heroes) {
                                        hero_modes::BatonEnemy heroList[our_ball::maxEnemies];
                                        std::array<world_trace::Vital, world_trace::enemySlots> heroVit{};
                                        const uint32_t heroVn = world_trace::enemyVitals(heroVit);
                                        for (uint32_t e = 0; e < foes.count; ++e) {
                                            heroList[e] = {foes.list[e].id, foes.list[e].feet + Vec3{0, 1.2f, 0}};
                                            for (uint32_t v = 0; v < heroVn && v < heroVit.size(); ++v)
                                                if (heroVit[v].id == foes.list[e].id && heroVit[v].have && heroVit[v].hp <= 0.f)
                                                    heroList[e].down = true;
                                        }
                                        hero007::PlayInput& hin = hero007In;
                                        hin.dt = hero007At < 0 ? 0.f : static_cast<float>(std::clamp(combatClock - hero007At, 0.0, .1));
                                        hero007At = combatClock;
                                        hin.play = cf.play;
                                        hin.hero = heroWas;
                                        hin.head = frame.head;
                                        hin.list = heroList;
                                        hin.count = foes.count;
                                        hin.flying = cf.flightTriggers;
                                        hin.chain = c38(combat38::batarangChain);
                                        hin.reload = c38(combat38::easyReload);
                                        hin.crossDraw = combat38::holsterCrossDraw(c38(combat38::holsterHands)); hin.eyeHeight = calibratedEye;
                                        hin.batonHolsters = combat38::batonHolstersOn(c38(combat38::batonHolsters));
                                        hin.shieldArm = c38(combat38::shieldArm); // Version 0.0.7 CAPTAIN AMERICA
                                        hin.shieldChain = captain::chainOn(c38(combat38::shieldChain));
                                        hin.shieldDamage = c38(combat38::shieldDamage);
                                        for (unsigned i = 0; i < 2; ++i) {
                                            hin.hands[i].position = cf.hands[i].position;
                                            hin.hands[i].aim = cf.hands[i].aim;
                                            hin.hands[i].tracked = cf.hands[i].tracked;
                                            hin.hands[i].trigger = cf.hands[i].button;
                                            // Version 0.0.72 WOLVERINE: the claws come out on the grip fist.
                                            hin.hands[i].squeeze = std::isfinite(frame.hands[i].squeeze) ? frame.hands[i].squeeze : 0.f;
                                            // Version 0.0.7 PUNISHER GUN GRIP: the held gun from the grip pose.
                                            hin.hands[i].holdGun(gun_grip::held(frame.hands[i].grip, i, runtime.controllers()[i]));
                                        }
                                        hero007::PlayOutput hout;
                                        hero007Play.step(hin, hout);
                                        for (unsigned i = 0; i < 2; ++i) // Version 0.0.7 BATMAN GRAPPLE GUN
                                            batmanGrapple[i] = hout.grapple[i];
                                        for (uint32_t k = 0; k < hout.blowCount; ++k) {
                                            const auto& hb = hout.blows[k];
                                            const bool sent = sm2_damage::queue(hb.id, hb.blow);
                                            (sent ? hero007Sent : hero007Unsent) += 1;
                                            hitMarks.add(hb.at);
                                            if (hero007Sent + hero007Unsent <= 8 || !sent) {
                                                char line[160];
                                                sprintf_s(line, "VERSION 0.0.7 %s HIT 0x%llx: %.0f HP blow %s",
                                                          hero007::blowKindNames[hb.kind < hero007::blowKinds ? hb.kind : 0],
                                                          static_cast<unsigned long long>(hb.id), static_cast<double>(hb.blow.amount),
                                                          sent ? "sent (CONFIRMED path)" : "not sent (damage path off)");
                                                game_prompts::note(line);
                                            }
                                        }
                                        for (unsigned i = 0; i < 2; ++i)
                                            if (hout.haptic[i] > 0)
                                                runtime.haptic(static_cast<int>(i), hout.haptic[i]);
                                        for (uint32_t id = 0; id < sfx::sounds; ++id) // Version 0.0.72: one a shot
                                            for (uint32_t k = hout.playsOf(id); k > 0; --k)
                                                sfx::play(static_cast<sfx::Id>(id));
                                        // Version 0.0.72 SOUND LOOPS: HEAT VISION / COLD BREATH as one seamless loop,
                                        // kept on each frame the beam is (a loop not refreshed stops by itself).
                                        sfx::loop(sfx::heatVision, hout.loopsOn(sfx::heatVision));
                                        sfx::loop(sfx::freezeBreath, hout.loopsOn(sfx::freezeBreath));
                                    } else {
                                        sfx::loop(sfx::heatVision, false); // Version 0.0.72: no hero beam, no loop
                                        sfx::loop(sfx::freezeBreath, false);
                                        batmanGrapple[0] = batmanGrapple[1] = false;
                                        hero007In.hero = heroWas;
                                        hero007In.batonHolsters = combat38::batonHolstersOn(c38(combat38::batonHolsters));
                                        hero007In.shieldArm = c38(combat38::shieldArm);
                                        hero007In.head = frame.head;
                                        hero007At = -1;
                                    }
                                    if (repMode) {
                                        // The press goes through the chord: a single press blasts after the
                                        // chord window, both pressed together and held fire the beam.
                                        bool press[2]{}, held[2]{};
                                        Vec3 aim[2]{};
                                        for (unsigned i = 0; i < 2; ++i) {
                                            press[i] = fightOut.ballOurs[i];
                                            // Version 0.0.62 PALM BLAST: along the palm's normal (repulsor::palmNormal),
                                            // the strong aim assist (oursBalls.fire) bending it onto a foe from the palm.
                                            const Pose& g = frame.hands[i].grip;
                                            const Vec3 palm = validTrackedPose(g) ? repulsor::palmNormal(g, i) : Vec3{};
                                            aim[i] = length(palm) > .5f ? palm : fightOut.ballDirection[i];
                                            held[i] = cf.hands[i].tracked && cf.hands[i].button > .5f;
                                        }
                                        const auto ro = repTrigger.update(static_cast<float>(combatClock), press, aim, held);
                                        // Version 0.0.6 (Update 39.0.2) REPULSOR: the take's charge whine on the press
                                        // (it fills the chord wait; the blast's thump is repulsor_fire).
                                        // Version 0.0.62 HAND COOLDOWN: a pull during that hand's 2 s makes no sound.
                                        for (unsigned i = 0; i < 2; ++i)
                                            if (ro.accepted[i] && repTrigger.chargingFor(static_cast<int>(i)) >= 0)
                                                sfx::play(sfx::repulsorCharge);
                                        // Strong aim assist always: STRONG, or MAX if picked.
                                        const uint32_t repStrength = strength == 1 ? 1u : 0u;
                                        for (unsigned i = 0; i < 2; ++i)
                                            if (ro.blast[i] && oursBalls.fire(static_cast<int>(i), palmOf(i), ro.blastAim[i], foes,
                                                                             fightOut.lockTarget, repStrength,
                                                                             our_ball::repulsorSpeed)) {
                                                runtime.haptic(static_cast<int>(i), .35f);
                                                sfx::play(sfx::repulsorFire); // Version 0.0.5 SOUND EFFECTS
                                                // Version 0.0.6 (Update 39.0.2): the palm's flash is the charge
                                                // fading out (repulsor_mcu palmGlow), not a splash.
                                            }
                                        if (ro.beam) {
                                            Vec3 from{}, dir{};
                                            if (repulsor::chestRay(cf.head, cf.forward, from, dir)) {
                                                Vec3 foeFeet[our_ball::maxEnemies];
                                                for (uint32_t e = 0; e < foes.count; ++e)
                                                    foeFeet[e] = foes.list[e].feet;
                                                // Version 0.0.6 BEAM AIM ASSIST: at the enemy nearest the look (30 deg).
                                                const int beamAimed = repulsor::beamAim(from, cf.forward, foeFeet,
                                                                                        static_cast<int>(foes.count), dir);
                                                const float beamNearest = repulsor::nearestMetres(from, foeFeet, static_cast<int>(foes.count));
                                                int hitIdx[our_ball::maxEnemies];
                                                const int beamCount = repulsor::beamHits(from, dir, foeFeet, static_cast<int>(foes.count), hitIdx,
                                                                                 our_ball::maxEnemies);
                                                // Version 0.0.62 BOLT WALLS: the beam's blows and its picture wait for its
                                                // wall (one cast, 1-3 frames; at most beamWaitSeconds, then the full
                                                // 0.0.6 beam - never stopped by a guess); the sound and buzz are now.
                                                const uint32_t beamSerialNext = beamWait.serial + 1u;
                                                beamWait = {};
                                                beamWait.waiting = true;
                                                beamWait.serial = beamSerialNext;
                                                beamWait.firedAt = combatClock;
                                                {
                                                    const Pose beamToWorld = compose(motion.headPose, inverse(frame.head));
                                                    beamWait.fromWorld = compose(beamToWorld, Pose{from, {}}).position;
                                                    const Vec3 dw = beamToWorld.orientation.rotate(dir);
                                                    beamWait.dirWorld = length(dw) > .5f ? normalized(dw) : Vec3{};
                                                }
                                                for (int k = 0; k < beamCount && beamWait.count < our_ball::maxEnemies; ++k)
                                                    if (hitIdx[k] >= 0 && static_cast<uint32_t>(hitIdx[k]) < foes.count) {
                                                        const auto& foe = foes.list[static_cast<size_t>(hitIdx[k])];
                                                        beamWait.ids[beamWait.count] = foe.id;
                                                        beamWait.along[beamWait.count++] = dot(foe.feet + Vec3{0, 1.f, 0} - from, dir);
                                                    }
                                                repBeamFrom = from;
                                                repBeamTo = from + dir * repulsor::beamRange;
                                                sfx::play(sfx::chestBeam); // Version 0.0.5 SOUND EFFECTS
                                                runtime.haptic(0, 1.f);
                                                runtime.haptic(1, 1.f);
                                                char line[240];
                                                char nearest[64];
                                                if (beamNearest < 0)
                                                    sprintf_s(nearest, "no enemy alive near");
                                                else
                                                    sprintf_s(nearest, "nearest enemy %.0f m", static_cast<double>(beamNearest));
                                                sprintf_s(line, "VERSION 0.0.6 CHEST BEAM: fired, %d enemies in the beam (%s, %s), %.0f HP each "
                                                                "(4x a blast, sent: %s); ready again in 30 s", beamCount,
                                                          beamAimed >= 0 ? "assist on one" : "no enemy in the 30 deg cone", nearest,
                                                          static_cast<double>(sm2_damage::chestBeam().amount),
                                                          sm2_damage::enabled() ? "yes" : "no, the damage path is closed");
                                                game_prompts::note(line);
                                            }
                                        }
                                        // Version 0.0.62 BOLT WALLS: the beam's wall came (or the wait ran out):
                                        // the beam reaches the wall, the enemies short of it take the blow.
                                        if (beamWait.waiting &&
                                            (beamWait.wall.known || combatClock - beamWait.firedAt >= beamWaitSeconds || !raysStarted)) {
                                            const float reach = bolt_walls::beamReach(beamWait.wall, repulsor::beamRange);
                                            int landed = 0, walled = 0;
                                            for (int k = 0; k < beamWait.count; ++k) {
                                                if (beamWait.along[k] > reach + .5f) { // behind the wall (half a metre's grace)
                                                    ++walled;
                                                    continue;
                                                }
                                                ++landed;
                                                repLedger.beam();
                                                if (sm2_damage::enabled() && beamWait.ids[k] &&
                                                    sm2_damage::queue(beamWait.ids[k], sm2_damage::chestBeam()))
                                                    ++repBlowsSent;
                                                for (uint32_t e = 0; e < foes.count; ++e)
                                                    if (foes.list[e].id == beamWait.ids[k])
                                                        hitMarks.add(foes.list[e].feet + Vec3{0, 1.f, 0});
                                            }
                                            repBeamHitsTotal += static_cast<uint64_t>(landed);
                                            const Vec3 beamDir = repBeamTo - repBeamFrom;
                                            if (length(beamDir) > .01f)
                                                repBeamTo = repBeamFrom + normalized(beamDir) * reach;
                                            repBeamAt = combatClock;
                                            (beamWait.wall.known && beamWait.wall.blocked ? beamClipped : beamUnclipped) += 1;
                                            char line[200];
                                            sprintf_s(line, "VERSION 0.0.62 CHEST BEAM WALL: %s, reach %.0f m; %d enemies hit, %d behind the wall",
                                                      beamWait.wall.known ? (beamWait.wall.blocked ? "a wall" : "clear") : "no answer (full beam, as 0.0.6)",
                                                      static_cast<double>(reach), landed, walled);
                                            game_prompts::note(line);
                                            beamWait.waiting = false;
                                        }
                                        if (ro.ready) {
                                            runtime.haptic(0, .3f);
                                            runtime.haptic(1, .3f);
                                            sfx::play(sfx::beamReady); // Version 0.0.5: the ready cue
                                            repReadyAt = combatClock;
                                            game_prompts::note("VERSION 0.0.6 CHEST BEAM READY");
                                        }
                                    } else {
                                        repTrigger.clear();
                                        beamWait.waiting = false; // Version 0.0.62: no held beam outlives IRON MAN
                                    }
                                    // Version 0.0.72 (TEST) (hero072_play.hpp): THOR, GREEN LANTERN, the list's
                                    // rules, reputation, suits, perks, ultimates and PERCH TAKEDOWN, every hero.
                                    // Every blow on sm2_damage's path (CONFIRMED) as hero007's; each feature's
                                    // guard turns only that feature off on a bad input (never a crash).
                                    {
                                        if (!hero072Loaded) {
                                            hero072Loaded = true;
                                            const std::string kept = readHero072Progress();
                                            if (!kept.empty())
                                                hero072Play.load(kept.c_str());
                                            hero072Play.seedLightning(static_cast<uint32_t>(GetTickCount64())); // THOR's sound pick
                                        }
                                        hero_modes::BatonEnemy list72[our_ball::maxEnemies];
                                        std::array<world_trace::Vital, world_trace::enemySlots> vit72{};
                                        const uint32_t vn72 = world_trace::enemyVitals(vit72);
                                        const uint32_t n72 = std::min<uint32_t>(foes.count, our_ball::maxEnemies);
                                        for (uint32_t e = 0; e < n72; ++e) {
                                            list72[e] = {foes.list[e].id, foes.list[e].feet + Vec3{0, 1.2f, 0}};
                                            for (uint32_t v = 0; v < vn72 && v < vit72.size(); ++v)
                                                if (vit72[v].id == foes.list[e].id && vit72[v].have && vit72[v].hp <= 0.f)
                                                    list72[e].down = true;
                                        }
                                        Vec3 puffs72[hero007::smokeMost]{};
                                        float ages72[hero007::smokeMost]{};
                                        for (uint32_t k = 0; k < hero007::smokeMost; ++k) {
                                            puffs72[k] = hero007Play.smoke.puffs[k].at;
                                            ages72[k] = hero007Play.smoke.puffs[k].age;
                                        }
                                        hero072::PlayIn in72;
                                        in72.dt = hero072At < 0 ? 0.f : static_cast<float>(std::clamp(combatClock - hero072At, 0.0, .1));
                                        hero072At = combatClock;
                                        in72.play = cf.play;
                                        in72.hero = heroWas;
                                        in72.head = frame.head;
                                        for (unsigned i = 0; i < 2; ++i) {
                                            in72.hands[i].position = cf.hands[i].position;
                                            in72.hands[i].aim = cf.hands[i].aim;
                                            in72.hands[i].tracked = cf.hands[i].tracked;
                                            hero072HandAt[i] = cf.hands[i].position; // 0.0.72 GRAPPLE's line
                                            hero072HandAim[i] = cf.hands[i].aim;
                                            hero072HandTracked[i] = cf.hands[i].tracked;
                                            in72.hands[i].trigger = cf.hands[i].button;
                                            in72.hands[i].grip = cf.hands[i].grip;
                                            // Test build 10 THOR HAMMER GRIP: Mjolnir in the fist from the grip pose.
                                            in72.hands[i].hammer = thor_grip::held(frame.hands[i].grip, runtime.controllers()[i]);
                                        }
                                        in72.list = list72;
                                        in72.count = n72;
                                        // The health lost since the last step (NaN: the health is not read).
                                        in72.lostShare = hero072LostKnown ? hero072Lost : std::numeric_limits<float>::quiet_NaN();
                                        hero072Lost = 0;
                                        in72.flying = cf.flightTriggers;
                                        in72.speed = in72.altitude = std::numeric_limits<float>::quiet_NaN();
                                        in72.clawsOut = hero007Play.clawsOut[0] || hero007Play.clawsOut[1];
                                        in72.shieldHand = hero007Play.shieldHand;
                                        in72.puffs = puffs72;
                                        in72.puffAges = ages72;
                                        in72.puffCount = hero007::smokeMost;
                                        in72.puffLife = hero007::smokeSeconds;
                                        // PERCH TAKEDOWN: a high spot is the target's height under the head (the
                                        // CANDIDATE stand-in for the game's perch, not found); the glide is the
                                        // cape bridge's. The quick press is Y, which reaches the game as always;
                                        // the game's takedown prompt is not read yet, so the press is COMING SOON.
                                        in72.perched = heroWas == hero_modes::batman;
                                        in72.gliding = heroWas == hero_modes::batman && capeNow.gliding != 0;
                                        in72.faceButton = (frame.buttons & buttonY) != 0;
                                        in72.jumpBusy = jumpBusy72 || (frame.buttons & buttonA) != 0;
                                        in72.takedownPrompt = false;
                                        in72.o = hero072Options();
                                        // Build 8 ROSTER page: the suits it shows (published only when they change), and
                                        // a suit picked on it (row 0: the player's own mod or the game's suit).
                                        {
                                            const bool all72 = hero072::testSuits(in72.o.testUnlocks);
                                            if (const uint32_t req = rosterSuitRequest.exchange(0)) {
                                                const uint32_t h = (req - 1) / 16, i = (req - 1) % 16;
                                                char line[128];
                                                if (hero072Play.suits.choose(h, i, all72)) {
                                                    if (i > 0 && in72.o.ownSuit) { // a suit of ours picked: OWN SUIT off to wear it
                                                        combat38Values[combat38::ownSuit].store(0);
                                                        in72.o.ownSuit = false;
                                                    }
                                                    hero072Play.progressDirty = true;
                                                    sprintf_s(line, "BUILD 8 ROSTER: suit %u of hero %u chosen", i, h);
                                                } else
                                                    sprintf_s(line, "BUILD 8 ROSTER: suit %u of hero %u refused (locked or MODEL MISSING)", i, h);
                                                game_prompts::note(line);
                                            }
                                            roster_page::publish(roster_page::snapshotOf(hero072Play.suits, all72, in72.o.ownSuit));
                                        }
                                        // HERO SUIT NEXT: one step to this hero's next unlocked suit, then back to DEFAULT.
                                        if (c38(combat38::heroSuit) == 1) {
                                            combat38Values[combat38::heroSuit].store(0);
                                            const uint32_t h72 = heroWas < hero_modes::heroes ? heroWas : 0u;
                                            const uint32_t worn = hero072Play.suits.next(h72, hero072::testSuits(in72.o.testUnlocks));
                                            hero072Play.progressDirty = true;
                                            char line[96];
                                            sprintf_s(line, "VERSION 0.0.72 HERO SUIT: suit %u of hero %u", worn, h72);
                                            game_prompts::note(line);
                                        }
                                        hero072::PlayOut out72;
                                        hero072Play.step(in72, out72);
                                        { // Build 9 SUITS: the picked suit's model worn (the render thread draws it)
                                            uint32_t ws = 0;
                                            // Test build 10 BATMAN BODY: the model worn (SuitCatalog::worn) - BATMAN's DEFAULT
                                            // wears his first unlocked loaded suit (the game has no BATMAN body).
                                            if (heroWas < hero_modes::heroes && heroWas != hero_modes::spiderMan)
                                                if (const hero072::Suit* x = hero072Play.suits.worn(
                                                        heroWas, out72.suit, hero072::testSuits(in72.o.testUnlocks), [](const char* f) {
                                                            const uint32_t k = mesh_model::suitSlot(f);
                                                            return k && suitsRigged.load() && mesh_model::library().model(k) &&
                                                                   suitRigs[k - mesh_model::slotSuitFirst].ok;
                                                        }))
                                                    ws = mesh_model::suitSlot(x->folder);
                                            if (wornSuitSlot.exchange(ws) != ws && ws) {
                                                char line[160];
                                                sprintf_s(line, "TEST " SPIDY_TEST_NUMBER " SUIT: wearing %s", mesh_model::slotFolders[ws]);
                                                game_prompts::note(line);
                                            }
                                        }
                                        for (uint32_t k = 0; k < 2; ++k) // Version 0.0.72: THOR's lightning, one sound a strike
                                            for (uint32_t n = out72.lightning[k]; n > 0; --n)
                                                sfx::play(k ? sfx::thorLightning2 : sfx::thorLightning1);
                                        hero072KeepTopped = out72.keepTopped;
                                        // Build 7 THOR: each of his moves named in the log (the first 12 of each kind).
                                        if (const uint32_t ev = hero072Play.takeThorEvents()) {
                                            static uint32_t thorSaid[9]{};
                                            static const char* const thorNames[9] = {
                                                "hammer drawn from the back", "hammer THROWN", "hammer RECALLED (grip)",
                                                "hammer CAUGHT", "hammer back on the back (not caught)", "thrown hammer HIT",
                                                "LIGHTNING BLAST (right trigger)", "STORM called (hammer raised)", "hammer MELEE hit"};
                                            for (uint32_t b = 0; b < 9; ++b)
                                                if ((ev >> b) & 1u && ++thorSaid[b] <= 12) {
                                                    char line[160];
                                                    if (b == 1)
                                                        sprintf_s(line, "VERSION 0.0.72 THOR: %s at %.1f m/s (throw %u)", thorNames[b],
                                                                  static_cast<double>(hero072Play.lastThrowSpeed), hero072Play.throws);
                                                    else
                                                        sprintf_s(line, "VERSION 0.0.72 THOR: %s", thorNames[b]);
                                                    game_prompts::note(line);
                                                }
                                        }
                                        for (unsigned i = 0; i < 2; ++i) { // Version 0.0.72 GRAPPLE
                                            hero072Grapple[i] = hero072Grapple[i] || out72.grapple[i];
                                            hero072HookOut[i] = out72.hookOut[i];
                                        }
                                        for (uint32_t k = 0; k < out72.blowCount; ++k) {
                                            const auto& b72 = out72.blows[k];
                                            const bool sent = sm2_damage::queue(b72.id, b72.blow);
                                            (sent ? hero072Sent : hero072Unsent) += 1;
                                            hitMarks.add(b72.at);
                                            if (hero072Sent + hero072Unsent <= 8 || !sent) {
                                                char line[160];
                                                sprintf_s(line, "VERSION 0.0.72 %s HIT 0x%llx: %.0f HP blow %s",
                                                          hero072::blowKindNames[b72.kind < hero072::blowKinds ? b72.kind : 0],
                                                          static_cast<unsigned long long>(b72.id), static_cast<double>(b72.blow.amount),
                                                          sent ? "sent (CONFIRMED path)" : "not sent (damage path off)");
                                                game_prompts::note(line);
                                            }
                                        }
                                        for (unsigned i = 0; i < 2; ++i)
                                            if (out72.haptic[i] > 0)
                                                runtime.haptic(static_cast<int>(i), out72.haptic[i]);
                                        if (out72.perch.comingSoon || out72.perch.sendButton) {
                                            ++hero072Perches;
                                            if (hero072Perches <= 8) {
                                                char line[128];
                                                sprintf_s(line, "VERSION 0.0.72 PERCH TAKEDOWN 0x%llx: %s",
                                                          static_cast<unsigned long long>(out72.perch.target),
                                                          out72.perch.comingSoon ? "COMING SOON (the game's prompt not read)"
                                                                                 : "the game's prompt button");
                                                game_prompts::note(line);
                                            }
                                        }
                                        if (out72.ultimateFired)
                                            game_prompts::note("VERSION 0.0.72 ULTIMATE fired");
                                        hero072Look = out72.look;
                                        hero072Look.hammerStamp = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
                                        hero072Look.blood = nullptr; // the draw never reads the game thread's blood
                                        // The progress kept: at most every 10 s, written whole (a crash keeps the last).
                                        if (hero072Play.progressDirty && (hero072SavedAt < 0 || combatClock - hero072SavedAt > 10.0)) {
                                            hero072SavedAt = combatClock;
                                            static char kept[16384];
                                            hero072Play.save(kept, sizeof kept);
                                            if (writeWhole(hero072ProgressPath(), kept))
                                                hero072Play.progressDirty = false;
                                        }
                                    }
                                    for (unsigned i = 0; i < 2 && heroWas == hero_modes::spiderMan; ++i) {
                                        if (!fightOut.ballOurs[i])
                                            continue;
                                        if (oursBalls.fire(static_cast<int>(i), wristOf(i), fightOut.ballDirection[i], foes,
                                                           fightOut.lockTarget, strength))
                                            runtime.haptic(static_cast<int>(i), .25f);
                                    }
                                    // Version 0.0.62 BOLT WALLS: each shot's wall in this frame's tracking space -
                                    // its own cast's, or until that comes the hand's aim ray when it left along it.
                                    bolt_walls::Wall wallsHere[our_ball::Pool::balls]{};
                                    {
                                        const Pose shotToWorld = compose(motion.headPose, inverse(frame.head));
                                        const Pose shotFromWorld = inverse(shotToWorld);
                                        for (int sl = 0; sl < our_ball::Pool::balls && finite(shotToWorld.position) &&
                                                         finite(shotFromWorld.position);
                                             ++sl) {
                                            const auto& b = oursBalls.ball(sl);
                                            if (!b.alive || !b.serial)
                                                continue;
                                            if (shotWallSerial[sl] != b.serial && (b.hand == 0 || b.hand == 1)) {
                                                const auto w = bolt_walls::fromHandRay(handWallsWorld[b.hand],
                                                                                       compose(shotToWorld, Pose{b.at, {}}).position,
                                                                                       shotToWorld.orientation.rotate(b.direction));
                                                if (w.known) {
                                                    shotWallsWorld[sl] = w;
                                                    shotWallSerial[sl] = b.serial;
                                                }
                                            }
                                            if (shotWallSerial[sl] != b.serial || !shotWallsWorld[sl].known)
                                                continue;
                                            bolt_walls::Wall w = shotWallsWorld[sl];
                                            w.from = compose(shotFromWorld, Pose{w.from, {}}).position;
                                            w.dir = shotFromWorld.orientation.rotate(w.dir);
                                            if (finite(w.from) && finite(w.dir) && std::abs(length(w.dir) - 1) < .01f)
                                                wallsHere[sl] = w;
                                        }
                                    }
                                    const auto step = oursBalls.update(frame.seconds, foes, wallsHere);
                                    // A shot stopped at a wall: its impact flash on the wall.
                                    for (int h = 0; h < step.wallCount && h < our_ball::Step::most; ++h) {
                                        oursSplash[oursSplashNext].start(step.walls[h].at);
                                        oursSplashNext = (oursSplashNext + 1) % our_ball::Pool::balls;
                                    }
                                    {
                                        uint64_t ids[our_ball::maxEnemies];
                                        Vec3 footAt[our_ball::maxEnemies];
                                        for (uint32_t e = 0; e < foes.count; ++e) {
                                            ids[e] = foes.list[e].id;
                                            footAt[e] = foes.list[e].feet;
                                        }
                                        cocoons.update(frame.seconds, ids, footAt, static_cast<int>(foes.count));
                                    }
                                    for (int h = 0; h < step.hitCount && h < our_ball::Step::most; ++h) {
                                        const auto& hit = step.hits[h];
                                        hitMarks.add(hit.at); // Version 0.0.62 HIT INDICATOR
                                        oursSplash[oursSplashNext].start(hit.at); // REPULSOR: the impact flash
                                        if (!plan.webSplash) { // REPULSOR: no web of any kind
                                            oursSplashNext = (oursSplashNext + 1) % our_ball::Pool::balls;
                                            repLedger.blast();
                                            // Version 0.0.6: the blast's blow, real damage while IRON MAN is on (rep0050 CONFIRMED).
                                            if (sm2_damage::enabled() && hit.index >= 0 &&
                                                static_cast<uint32_t>(hit.index) < foes.count &&
                                                sm2_damage::queue(foes.list[static_cast<size_t>(hit.index)].id,
                                                                  sm2_damage::repulsorBlast()))
                                                ++repBlowsSent;
                                            if (hit.hand == 0 || hit.hand == 1)
                                                runtime.haptic(hit.hand, .7f);
                                            continue;
                                        }
                                        if (hit.index >= 0 && static_cast<uint32_t>(hit.index) < foes.count &&
                                            cocoons.hit(hit.enemy, foes.list[static_cast<size_t>(hit.index)].feet)) {
                                            char line[160];
                                            sprintf_s(line,
                                                      "Version 0.0.3 WEB BALL: enemy webbed up (the mod's look; %llu so far, "
                                                      "the game's webbing CANDIDATE, not sent)",
                                                      static_cast<unsigned long long>(cocoons.webbedUp()));
                                            game_prompts::note(line);
                                        }
                                        oursSplashNext = (oursSplashNext + 1) % our_ball::Pool::balls;
                                        // Version 0.0.5 WEB BALL DAMAGE (DAMAGE TEST, OFF by default): the web ball's small blow,
                                        // only with that switch on (0.0.6: the heroes open the path for their own hits).
                                        if (c38(combat38::heroDamage) == 1 && sm2_damage::enabled() && hit.index >= 0 &&
                                            static_cast<uint32_t>(hit.index) < foes.count)
                                            sm2_damage::queue(foes.list[static_cast<size_t>(hit.index)].id, sm2_damage::webBallTest());
                                        if (hit.hand == 0 || hit.hand == 1) {
                                            runtime.haptic(hit.hand, .7f);
                                            if (plan.gameWebOnHit && hit.index >= 0 &&
                                                static_cast<uint32_t>(hit.index) < foes.count) {
                                                combatMachine.hitBall(static_cast<unsigned>(hit.hand), cf.head, cf.forward,
                                                                      foes.list[static_cast<size_t>(hit.index)].feet);
                                                ++oursGameHits;
                                            }
                                        }
                                    }
                                    for (auto& sp : oursSplash)
                                        sp.update(frame.seconds);
                                    // Version 0.0.62 HIT INDICATOR: the marks age; OFF keeps none.
                                    hitMarks.update(frame.seconds);
                                    if (c38(combat38::hitIndicator) != 1)
                                        hitMarks.clear();
                                    if (repMode && repTrigger.blasts() != repNotedBlasts && combatClock - repNotedAt >= 2) {
                                        repNotedBlasts = repTrigger.blasts();
                                        repNotedAt = combatClock;
                                        char line[420];
                                        sprintf_s(line,
                                                  "VERSION 0.0.6 REPULSOR: blasts %llu, hits %llu, beams %llu (%llu enemies), chords %llu, "
                                                  "short %llu, while cooling %llu; hits counted %.0f blast + %.0f beam, "
                                                  "blows queued %llu (real damage), 0.0.62 hand cooldown ignored %llu",
                                                  static_cast<unsigned long long>(repTrigger.blasts()),
                                                  static_cast<unsigned long long>(repLedger.blastHits),
                                                  static_cast<unsigned long long>(repTrigger.beams()),
                                                  static_cast<unsigned long long>(repBeamHitsTotal),
                                                  static_cast<unsigned long long>(repTrigger.chords()),
                                                  static_cast<unsigned long long>(repTrigger.shortChords()),
                                                  static_cast<unsigned long long>(repTrigger.cooledChords()),
                                                  static_cast<double>(repLedger.blastTotal), static_cast<double>(repLedger.beamTotal),
                                                  static_cast<unsigned long long>(repBlowsSent),
                                                  static_cast<unsigned long long>(repTrigger.handCooledPresses()));
                                        game_prompts::note(line);
                                    }
                                    if (oursBalls.fired() != oursNotedFired && combatClock - oursNotedAt >= 2) {
                                        oursNotedFired = oursBalls.fired();
                                        oursNotedAt = combatClock;
                                        char line[320];
                                        sprintf_s(line,
                                                  "Version 0.0.2 OURS BALL: fired %llu (left %llu, right %llu), hits %llu, "
                                                  "misses %llu, bent %llu, straight %llu, refused %llu, last off %.0f deg, "
                                                  "AIM STRENGTH %s, game balls asked %llu, webbed up %llu (mod look)",
                                                  static_cast<unsigned long long>(oursBalls.fired()),
                                                  static_cast<unsigned long long>(oursBalls.firedBy(0)),
                                                  static_cast<unsigned long long>(oursBalls.firedBy(1)),
                                                  static_cast<unsigned long long>(oursBalls.hits()),
                                                  static_cast<unsigned long long>(oursBalls.misses()),
                                                  static_cast<unsigned long long>(oursBalls.assisted()),
                                                  static_cast<unsigned long long>(oursBalls.unassisted()),
                                                  static_cast<unsigned long long>(oursBalls.refused()),
                                                  static_cast<double>(oursBalls.lastDegrees()), our_ball::strengthName(strength),
                                                  static_cast<unsigned long long>(oursGameHits),
                                                  static_cast<unsigned long long>(cocoons.webbedUp()));
                                        game_prompts::note(line);
                                    }
                                } catch (...) {
                                    oursFault = true;
                                    oursBalls.clear();
                                    cocoons.clear();
                                    repTrigger.clear();
                                    game_prompts::note("Version 0.0.2: OURS's web balls failed and turned themselves off");
                                }
                            }
                            pad.buttons |= fightOut.buttons;
                            // Update 38: the fight's buttons in flight go to the game (askingGame
                            // hands it the player), and the CRAWL CAP holds off after any input.
                            const bool fought = fightOut.buttons != 0 || fightOut.punch[0] || fightOut.punch[1] ||
                                                fightOut.fired[0] != 0 || fightOut.fired[1] != 0 || fightOut.zip;
                            askingGame = askingGame || (inFlight && fought);
                            combatInputLast = fought;
                            combatClock += std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                            // Update 38.3 GADGET HINT: name what each gesture fired over its wrist; after
                            // a missed Sonic pose, how to make it.
                            {
                                const bool hintsOn = c38(combat38::gadgetHint) == 0;
                                for (unsigned i = 0; i < 2; ++i) {
                                    gadgetHints[i].update(frame.seconds);
                                    const uint32_t g = fightOut.fired[i];
                                    const uint32_t label = g == combat_input::upshot       ? combat38::hintUpshot
                                                           : g == combat_input::webGrabber  ? combat38::hintGrabber
                                                           : g == combat_input::sonicBurst  ? combat38::hintSonic
                                                           : g == combat_input::ricochetWeb ? combat38::hintRicochet
                                                           : g == combat_input::webYank     ? combat38::hintYank
                                                                                            : combat38::hintNone;
                                    if (hintsOn && label != combat38::hintNone) {
                                        gadgetHints[i].show(label);
                                        ++gadgetHintsShown;
                                    }
                                }
                                const uint64_t missed = combatMachine.blastMissed();
                                if (hintsOn && missed > sonicMissedSeen && combatClock - sonicHintAt >= combat38::GadgetHint::sonicEvery) {
                                    sonicHintAt = combatClock;
                                    gadgetHints[0].show(combat38::hintSonicHow);
                                    gadgetHints[1].label = combat38::hintNone;
                                    ++sonicHintsShown;
                                }
                                sonicMissedSeen = missed;
                                if (!hintsOn)
                                    gadgetHints[0].label = gadgetHints[1].label = combat38::hintNone;
                            }
                            for (unsigned i = 0; i < 2; ++i) {
                                globs[i].update(frame.seconds);
                                // Update 38 BALL LOOK GLOB: our glob flies with the web ball.
                                // Update 38.3: GLOB is 1 (GAME ONLY, 0, the default: the game's ball alone).
                                if (fightOut.fired[i] == combat_input::webShooter && c38(combat38::ballLook) == combat38::lookGlob &&
                                    heroNow() == hero_modes::spiderMan) { // 0.0.4/0.0.6: no glob for any other hero
                                    // Update 38.1: from the web shooter's nozzle (AIM AXIS HAND), 38: the grip.
                                    const Vec3 from = c38(combat38::aimAxis) == 0 && validTrackedPose(frame.hands[i].grip)
                                                          ? spidy::webShooter(frame.hands[i].grip, i).position
                                                          : cf.hands[i].position;
                                    globs[i].launch(from, fightOut.ballDirection[i], fightOut.ballMetres[i]);
                                    ++globsLaunched;
                                }
                                // Update 38: why a punch was not sent, noted (once a second at most).
                                if (fightOut.candidate[i] && fightOut.reject[i] != combat38::sent &&
                                    fightOut.reject[i] < combat38::rejects) {
                                    lastReject = fightOut.reject[i];
                                    if (combatClock - rejectNoted >= 1 && fightOut.reject[i] != combat38::gated) {
                                        rejectNoted = combatClock;
                                        char line[128];
                                        sprintf_s(line, "Update 38: punch not sent (%s hand): %s", i ? "right" : "left",
                                                  combat38::rejectNames[fightOut.reject[i]]);
                                        game_prompts::note(line);
                                    }
                                }
                            }
                            if (fightOut.assisted)
                                assistLast = fightOut.assistDegrees;
                            // Update 37.2: the aim never overrides a stick the player is holding.
                            const bool stickHeld = std::abs(static_cast<int>(pad.thumbLX)) > 8000 ||
                                                   std::abs(static_cast<int>(pad.thumbLY)) > 8000;
                            if (fightOut.steering && !stickHeld) {
                                pad.thumbLX = static_cast<int16_t>(std::clamp(fightOut.stickX, -1.f, 1.f) * 32767);
                                pad.thumbLY = static_cast<int16_t>(std::clamp(fightOut.stickY, -1.f, 1.f) * 32767);
                            }
                            // Update 38.2 MELEE ANIM PUNCH ONLY: the X tap goes with the stick at rest (a
                            // stick held turns the game's light attack into its kick/launcher strings).
                            if (fightOut.stickNeutral) {
                                pad.thumbLX = 0;
                                pad.thumbLY = 0;
                            }
                            for (unsigned i = 0; i < 2; ++i) {
                                u37.holdsGrip[i] = fightOut.holdsGrip[i];
                                // Update 38.2 GADGET FEEL EASY: a short sharp buzz the moment a gadget is
                                // recognised (the fire's own buzz follows).
                                if (fightOut.recognised[i])
                                    runtime.haptic(static_cast<int>(i), .8f);
                                if (const auto g = fightOut.fired[i]) {
                                    runtime.haptic(static_cast<int>(i), g == combat_input::webShooter ? .25f : .5f);
                                    feel(g == combat_input::webShooter ? body_haptics::Kind::webShot
                                                                       : body_haptics::Kind::gadget,
                                         static_cast<int>(i), g == combat_input::webShooter ? .4f : .7f);
                                    if (g != combat_input::sonicBurst || i == 0) {
                                        char line[128];
                                        const uint32_t slot = g == combat_input::webGrabber ? config37.grabberSlot
                                                            : g == combat_input::sonicBurst ? config37.sonicSlot
                                                            : g == combat_input::upshot     ? config37.upshotSlot
                                                            : g == combat_input::ricochetWeb ? config37.ricochetSlot
                                                            : g == combat_input::webYank     ? config37.yankSlot
                                                                                             : 0;
                                        sprintf_s(line, "Update 37: %s gesture -> the game's %s", combat_input::gadgetNames[g],
                                                  g == combat_input::webShooter ? (heroNow() != hero_modes::spiderMan
                                                                                     ? "nothing (no webs for this hero)"
                                                                                     : fightOut.ballOurs[i] ? "nothing (OURS: the mod's own ball, no RB)" : "RB tap")
                                                                                : combat_input::slotName(slot));
                                        game_prompts::note(line);
                                    }
                                }
                                if (fightOut.punch[i]) {
                                    // Update 37: BLOOD SPLATTER plan B - a punch with an enemy near (no hit check in SM2).
                                    if (bloodIntensity.load() &&
                                        world_trace::nearestEnemyMetres() <= blood_splatter::nearMetres) {
                                        bloodPending |= 1u << i;
                                        // Update 38.2 BRUTAL: the punching glove takes a coat.
                                        // Update 38.5 EXTREME: always a coat, twice as heavy.
                                        const bool extremeHit = blood_splatter::isExtreme(bloodIntensity.load());
                                        if (c38(combat38::bloodLook) == combat38::bloodBrutal || extremeHit) {
                                            bloodCoats[i].hit();
                                            if (extremeHit)
                                                bloodCoats[i].hit();
                                            ++bloodCoatsMade;
                                        }
                                        bloodAim[i] = cf.hands[i].aim;
                                        // BLOOD LOOK BIG: at the enemy nearest the fist (within 3 m of it).
                                        bloodAt[i] = Vec3{NAN, NAN, NAN};
                                        bloodFloor[i] = NAN;
                                        float best = blood_splatter::nearMetres;
                                        for (uint32_t e = 0; e < cf.enemyCount; ++e) {
                                            const Vec3 middle = cf.enemies[e] + Vec3{0, 1.1f, 0};
                                            const float d = length(middle - cf.hands[i].position);
                                            if (finite(middle) && d < best) {
                                                best = d;
                                                bloodAt[i] = middle;
                                                bloodFloor[i] = cf.enemies[e].y;
                                            }
                                        }
                                    }
                                    runtime.haptic(static_cast<int>(i), .4f);
                                    feel(body_haptics::Kind::punch, static_cast<int>(i), .6f);
                                    // Version 0.0.6 DAREDEVIL PUNCH / HULK: a landed punch (an enemy's chest within
                                    // 2.2 m of the fist) gets one more blow on sm2_damage's path (CONFIRMED):
                                    // DAREDEVIL (k - 1) x a punch (each hits like k); HULK a normal enemy's whole
                                    // health (one punch KO), a brute's most / 2.5, with the big knock (fling, CANDIDATE).
                                    // Version 0.0.7 SUPER STRENGTH: SUPERMAN's landed punch gets one more blow (2 punches'
                                    // worth, a big knock) on the same CONFIRMED path; the knock's fling is CANDIDATE as HULK's.
                                    if (const uint32_t hero = heroNow(); hero == hero_modes::superman) {
                                        Vec3 punchFeet[world_trace::enemySlots];
                                        const uint32_t pn = std::min<uint32_t>(cf.enemyCount, world_trace::enemySlots);
                                        for (uint32_t e = 0; e < pn; ++e)
                                            punchFeet[e] = cf.enemies[e];
                                        const int victim = hero_modes::punchVictim(cf.hands[i].position, punchFeet, pn);
                                        if (victim < 0) {
                                            ++heroPunchMisses;
                                        } else {
                                            hitMarks.add(punchFeet[victim] + Vec3{0, 1.1f, 0});
                                            const uint64_t who = cf.enemyIds[static_cast<size_t>(victim)];
                                            const bool sent = who && sm2_damage::queue(who, hero007::superPunch());
                                            superPunchBlows += sent ? 1 : 0;
                                            if (superPunchBlows <= 5 || !sent) {
                                                char line[160];
                                                sprintf_s(line, "VERSION 0.0.7 SUPERMAN PUNCH 0x%llx: blow %s",
                                                          static_cast<unsigned long long>(who),
                                                          sent ? "sent (CONFIRMED path)" : "not sent (damage path off)");
                                                game_prompts::note(line);
                                            }
                                        }
                                    }
                                    if (const uint32_t hero = heroNow(); hero == hero_modes::daredevil || hero == hero_modes::hulk) {
                                        Vec3 punchFeet[world_trace::enemySlots];
                                        const uint32_t pn = std::min<uint32_t>(cf.enemyCount, world_trace::enemySlots);
                                        for (uint32_t e = 0; e < pn; ++e)
                                            punchFeet[e] = cf.enemies[e];
                                        const int victim = hero_modes::punchVictim(cf.hands[i].position, punchFeet, pn);
                                        if (victim < 0) {
                                            ++heroPunchMisses;
                                        } else {
                                            hitMarks.add(punchFeet[victim] + Vec3{0, 1.1f, 0});
                                            const uint64_t who = cf.enemyIds[static_cast<size_t>(victim)];
                                            sm2_damage::Blow blow{};
                                            if (hero == hero_modes::daredevil) {
                                                blow.amount = hero_modes::daredevilExtraHp(
                                                    combat38::daredevilPunchK(c38(combat38::daredevilPunch)));
                                            } else {
                                                float most = NAN;
                                                std::array<world_trace::Vital, world_trace::enemySlots> vit{};
                                                const uint32_t vn = world_trace::enemyVitals(vit);
                                                for (uint32_t v = 0; v < vn && v < vit.size(); ++v)
                                                    if (vit[v].id == who && vit[v].have)
                                                        most = vit[v].most;
                                                blow = {hero_modes::hulkPunchHp(most), 5, hero_modes::hulkFlingKnock};
                                            }
                                            const bool sent = who && blow.amount > 0 && sm2_damage::queue(who, blow);
                                            (hero == hero_modes::daredevil ? ddPunchBlows : hulkPunchBlows) += sent ? 1 : 0;
                                            char line[200];
                                            sprintf_s(line, "VERSION 0.0.6 %s PUNCH 0x%llx: +%.0f HP blow %s",
                                                      hero == hero_modes::daredevil ? "DAREDEVIL" : "HULK",
                                                      static_cast<unsigned long long>(who), static_cast<double>(blow.amount),
                                                      sent ? "sent (CONFIRMED path)" : blow.amount > 0 ? "not sent (damage path off)" : "none (1X)");
                                            game_prompts::note(line);
                                        }
                                    }
                                    // Version 0.0.62 PUNCH HITS (the SpidyVR comparison: its land() sends the
                                    // blow the moment the fist touches; ours waited for the game's X move to
                                    // reach the enemy): GAME + INSTANT / INSTANT ONLY send a small blow with a
                                    // knock on sm2_damage's CONFIRMED path on this frame - to the enemy the
                                    // fist's swept contact touched (its event), or for a fast-swing blow the
                                    // nearest chest within 2.2 m of the fist. Spider-Man and Iron Man only
                                    // (DAREDEVIL and HULK have their own extra blow above); never for the
                                    // zip strike (the game's Y lands that one).
                                    if (const uint32_t hero = heroNow(); !fightOut.zip && hero != hero_modes::daredevil &&
                                                                         hero != hero_modes::hulk && hero != hero_modes::superman &&
                                                                         combat38::instantPunch(c38(combat38::punchHits))) {
                                        uint64_t who = fightOut.punchTarget[i];
                                        Vec3 markAt{NAN, NAN, NAN};
                                        for (uint32_t e = 0; who && e < cf.enemyCount; ++e)
                                            if (cf.enemyIds[e] == who)
                                                markAt = cf.enemies[e] + Vec3{0, 1.1f, 0};
                                        if (!who || !finite(markAt)) {
                                            Vec3 punchFeet[world_trace::enemySlots];
                                            const uint32_t pn = std::min<uint32_t>(cf.enemyCount, world_trace::enemySlots);
                                            for (uint32_t e = 0; e < pn; ++e)
                                                punchFeet[e] = cf.enemies[e];
                                            const int victim = hero_modes::punchVictim(cf.hands[i].position, punchFeet, pn);
                                            who = victim >= 0 ? cf.enemyIds[static_cast<size_t>(victim)] : 0;
                                            if (victim >= 0)
                                                markAt = punchFeet[victim] + Vec3{0, 1.1f, 0};
                                        }
                                        if (!who) {
                                            ++instantMisses;
                                        } else {
                                            const auto ib = combat38::instantBlowOf(fightOut.punchStrength[i], c38(combat38::punchHits));
                                            const sm2_damage::Blow blow{ib.amount, 5, ib.knockAmount};
                                            const bool sent = sm2_damage::queue(who, blow);
                                            instantBlows += sent ? 1 : 0;
                                            hitMarks.add(markAt);
                                            if (sent)
                                                runtime.haptic(static_cast<int>(i), .7f); // the hit itself, on top
                                            if (instantBlows <= 3 || !sent) {
                                                char line[200];
                                                sprintf_s(line, "VERSION 0.0.62 PUNCH HITS 0x%llx: %.0f HP, knock %.0f %s",
                                                          static_cast<unsigned long long>(who), static_cast<double>(blow.amount),
                                                          static_cast<double>(blow.knockAmount),
                                                          sent ? "sent at contact (CONFIRMED path)" : "not sent (damage path closed)");
                                                game_prompts::note(line);
                                            }
                                        }
                                    }
                                    if (fightOut.zip) {
                                        char line[128];
                                        sprintf_s(line, "Update 37.2: ZIP STRIKE, enemy %.1f m -> the game's Y",
                                                  static_cast<double>(cf.enemyMetres));
                                        game_prompts::note(line);
                                    }
                                }
                            }
                            publish(SpidyCombatData, [&](combat_input::Data& d) {
                                d.on = config37.on;
                                d.tapMs = config37.tapMs;
                                d.gestureMs = config37.gestureMs;
                                d.slots = config37.grabberSlot | config37.sonicSlot << 4 | config37.upshotSlot << 8 |
                                          config37.ricochetSlot << 12 | config37.yankSlot << 16;
                                d.flags = (config37.punch ? 2u : 0u) | (config37.blastPalms ? 4u : 0u) |
                                          (config37.steer ? 8u : 0u) | (config37.zipStrike ? 16u : 0u) |
                                          (config37.webYank ? 32u : 0u);
                                d.jumps = combatMachine.jumps();
                                for (uint32_t g = 0; g < combat_input::gadgets; ++g)
                                    d.fired[g] = combatMachine.fired(g);
                                for (unsigned i = 0; i < 2; ++i) {
                                    d.punches[i] = combatMachine.punches(i);
                                    d.peakSpeed[i] = std::max(d.peakSpeed[i], fightOut.speed[i]);
                                }
                                d.blastMissed = combatMachine.blastMissed();
                                d.yankSpeed = config37.yankSpeed;
                                d.upSpeed = config37.upSpeed;
                                d.lowDegrees = config37.lowDegrees;
                                d.flickSpeed = config37.flickSpeed;
                                d.blastGap = config37.blastGap;
                                d.blastReach = config37.blastReach;
                                d.punchSpeed = config37.punchSpeed;
                                d.aimCone = config37.aimCone;
                                d.lockStick = config37.lockStick;
                                d.loose = config37.loose;
                                d.lastAim = fightOut.aimAngle;
                                d.steers += fightOut.steering;
                                d.gated += !cf.play;
                            });
                            {   // Update 38: the COMBAT record (CMB8, tools/update37_records.py 'combat38').
                                const uint32_t npcMode = std::min<uint32_t>(npc_fade::now() & 0xffu, 2u);
                                const bool enemyNear = cf.enemyMetres < 2.f;
                                const auto& holo = wristToggle.diagnostics();
                                const auto& tilt = rig.tiltGuard();
                                publish(SpidyCombat38Data, [&](combat38::Data& d) {
                                    d.flags = 1u;
                                    for (uint32_t o = 0; o < combat38::options && o < 28; ++o)
                                        (o < 12 ? d.values[o] : o < 20 ? d.values2[o - 12] : d.values3[o - 20]) = c38(o);
                                    d.aimStrengthNow = c38(combat38::aimStrength); // Version 0.0.2 (options 28, 29)
                                    d.ballHitNow = c38(combat38::ballHit);
                                    // Update 38.2 (version 4).
                                    d.forearmOpens = holo.forearmClicks;
                                    d.ringHeld = ringHold.held;
                                    d.targetedHeldOff = combatMachine.targetedHeldOff();
                                    // Version 0.0.4: SPACED's and EVERY HIT's together (one of them runs).
                                    d.meleeTaps = combatMachine.melee().taps() + combatMachine.everyHit().taps();
                                    d.meleeDelayed = combatMachine.melee().delayed() + combatMachine.everyHit().delayed();
                                    d.meleeDropped = combatMachine.melee().dropped();
                                    // Version 0.0.3 (rep0023's missed punches).
                                    d.meleeMerged = static_cast<uint32_t>(std::min<uint64_t>(
                                        combatMachine.melee().merged() + combatMachine.everyHit().merged(), 0xffffffffu));
                                    d.heldOffFar = static_cast<uint32_t>(std::min<uint64_t>(combatMachine.heldOffFar(), 0xffffffffu));
                                    d.gadgetsRecognised = combatMachine.recognised();
                                    d.grabAttached = grabWebs[0].attached + grabWebs[1].attached;
                                    d.grabYanks = grabWebs[0].yanks + grabWebs[1].yanks;
                                    d.grabReleased = grabWebs[0].released + grabWebs[1].released;
                                    d.smartAssists = combatMachine.smartAssists();
                                    d.bloodCoats = bloodCoatsMade;
                                    d.mirrorSuitFrames = mirrorSuitFrames;
                                    d.ringWristCm = ringWristNow * 100.f;
                                    d.ringWristPeakCm = ringWristPeak * 100.f;
                                    d.mirrorJoints = mirrorJointsLast;
                                    // Update 38.3 (version 5).
                                    d.lookSwitches = combatMachine.lookSwitches();
                                    d.strikeSteers = combatMachine.strikeSteers();
                                    d.grabAssisted = grabAssisted;
                                    d.suitSamples = suitColours.samples();
                                    d.suitSnapshots = suitColours.copies();
                                    d.suitPrimary = vr_mirror::packSrgb(suitPalette.primary);
                                    d.suitSecondary = vr_mirror::packSrgb(suitPalette.secondary);
                                    d.suitEmblem = vr_mirror::packSrgb(suitPalette.emblem);
                                    d.suitLearned = suitPalette.learned ? 1u : 0u;
                                    d.gadgetHints = gadgetHintsShown;
                                    d.sonicHints = sonicHintsShown;
                                    d.yankGestures = combatMachine.fired(combat_input::webYank);
                                    {
                                        uint64_t calls{}, writes{};
                                        float gameMetres{}, ourMetres{};
                                        SpidyFarWindowsStats(&calls, &writes, &gameMetres, &ourMetres);
                                        d.windowCalls = calls;
                                        d.windowWrites = writes;
                                        d.windowGameMetres = gameMetres;
                                        d.windowMetres = ourMetres;
                                    }
                                    // Update 38.1 (version 3): per-swing rejects, the holo zone, the ring
                                    // hidden as stale, STEADY lunges held off, lock switches held back.
                                    d.swingRejects = combatMachine.swingRejects();
                                    d.zoneSwallowed = holo.zoneSwallowed;
                                    d.holoTaps = holo.taps;
                                    d.holoLookOpens = holo.lookOpens;
                                    d.ringHidden = ringHiddenFrames;
                                    d.lungeHeldOff = combatMachine.lungeHeldOff();
                                    d.lockHeld = static_cast<uint32_t>(combatMachine.lockHeld());
                                    for (uint32_t r = 0; r < combat38::rejects; ++r)
                                        d.rejects[r] = combatMachine.rejects(r);
                                    d.contactHits = combatMachine.contactHits();
                                    d.gestureHits = combatMachine.gestureHits();
                                    d.zips = combatMachine.fired(combat_input::zipStrike);
                                    d.zipsAimed = combatMachine.zipsAimed();
                                    d.airPunches = combatMachine.airPunches();
                                    d.assists = combatMachine.assists();
                                    d.assistsMissed = combatMachine.assistsMissed();
                                    d.globs = globsLaunched;
                                    d.crawlCaps = crawlGuard.caps();
                                    d.crawlHeldOff = crawlGuard.heldOff();
                                    d.crawlWhy = crawlGuard.why();
                                    d.holoDeferred = holo.deferred;
                                    d.holoCancelled = holo.cancelled;
                                    d.tiltResets = tilt.resets;
                                    d.tiltCapped = tilt.capped;
                                    d.enemyNear[npcMode] += enemyNear && !enemyNearWas;
                                    d.enemyNearFrames[npcMode] += enemyNear;
                                    d.enemies = cf.enemyCount;
                                    d.lastReject = lastReject;
                                    d.npcMode = npcMode;
                                    d.nearestEnemy = std::isfinite(cf.enemyMetres) ? cf.enemyMetres : 1e9f;
                                    d.assistDegrees = assistLast;
                                    d.ringError = ringErrorNow;
                                    d.ringErrorPeak = ringErrorPeak;
                                    d.scanHz = world_trace::enemyScanHz();
                                    d.roofMetres = headroomSeen;
                                    d.lunges = combatMachine.lunges() +
                                              (lungeState.opened >= lungeOpenedBase ? lungeState.opened - lungeOpenedBase : 0);
                                    d.lungesLanded = combatMachine.lungesLanded();
                                    d.lungesMissed = lungeState.missed >= lungeMissedBase ? lungeState.missed - lungeMissedBase : 0;
                                    d.yanks = combatMachine.yanks();
                                    d.lockChanges = combatMachine.lockChanges();
                                    d.sightRays = sightRays;
                                    d.sightBlocked = sightBlocked;
                                    d.sightUnknown = combatMachine.sightUnknown();
                                });
                                enemyNearWas = enemyNear;
                            }
                            {   // Version 0.0.5 DAMAGE PROBE (read-only, 10 a second) and DAMAGE TEST's state.
                                damageProbeClock += std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                                if (damageProbeClock >= .1f) {
                                    damageProbeClock = 0;
                                    // Version 0.0.6: the damage path is open for any hero's own hits (rep0050 CONFIRMED it);
                                    // WEB BALL DAMAGE (DAMAGE TEST) opens it for Spider-Man's web ball.
                                    // Version 0.0.62 PUNCH HITS GAME + INSTANT / INSTANT ONLY opens it for Spider-Man's punches.
                                    sm2_damage::setEnabled(c38(combat38::heroDamage) == 1 || heroNow() != hero_modes::spiderMan ||
                                                           combat38::instantPunch(c38(combat38::punchHits)));
                                    // Version 0.0.5 SFX VOLUME (live) and the player's state in the log.
                                    sfx::setGain(combat38::sfxGainOf(c38(combat38::sfxVolume)));
                                    const sfx::Stats sfxNow = sfx::stats();
                                    if (sfxNow.state != sfxStateNoted && sfxNow.state != sfx::starting) {
                                        sfxStateNoted = sfxNow.state;
                                        char line[160];
                                        sprintf_s(line, "VERSION 0.0.5 SFX %s%s", sfx::stateNames[sfxNow.state < sfx::states ? sfxNow.state : 0],
                                                  sfxNow.state == sfx::playing ? " (XAudio2, its own thread)" : " (silent; the game is not affected)");
                                        game_prompts::note(line);
                                    }
                                    sm2_damage::setHero(world_trace::heroRecordNow());
                                    std::array<world_trace::Vital, world_trace::enemySlots> vitals{};
                                    const uint32_t vitalCount = world_trace::enemyVitals(vitals);
                                    damage_probe::Sample samples[world_trace::enemySlots]{};
                                    uint32_t withHealth{};
                                    for (uint32_t v = 0; v < vitalCount && v < world_trace::enemySlots; ++v) {
                                        samples[v] = {vitals[v].id, vitals[v].hp, vitals[v].most, vitals[v].have != 0};
                                        withHealth += vitals[v].have != 0;
                                    }
                                    const auto ds = sm2_damage::stats();
                                    damageTracker.update(GetTickCount64(), samples, vitalCount, ds.lastMeleeMs, ds.lastSentMs,
                                                         ds.sent > 0);
                                    if (damageTracker.writeState == 2)
                                        sm2_damage::markConfirmed();
                                    if (damageTracker.nextEvent - damageEventsNoted > damage_probe::events)
                                        damageEventsNoted = damageTracker.nextEvent - damage_probe::events;
                                    static constexpr const char* causes[3] = {"no X press or blow", "X press", "the mod's blow"};
                                    for (; damageEventsNoted < damageTracker.nextEvent; ++damageEventsNoted) {
                                        const auto& e = damageTracker.recent[damageEventsNoted % damage_probe::events];
                                        char line[200];
                                        sprintf_s(line, "VERSION 0.0.5 DAMAGE PROBE: enemy HP %.1f -> %.1f of %.0f, %s%s (%u ms)",
                                                  static_cast<double>(e.before), static_cast<double>(e.after),
                                                  static_cast<double>(e.most), causes[e.cause % 3],
                                                  e.knockedOut ? ", KNOCKED OUT" : "", e.delayMs);
                                        game_prompts::note(line);
                                    }
                                    if (damageTracker.healthState != damageHealthNoted) {
                                        damageHealthNoted = damageTracker.healthState;
                                        game_prompts::note(damageHealthNoted == 2
                                                               ? "VERSION 0.0.5 DAMAGE PROBE: punch damage CONFIRMED (enemy HP fell after X)"
                                                               : "VERSION 0.0.5 DAMAGE PROBE: enemy HP read");
                                    }
                                    if (damageTracker.writeState != damageWriteNoted) {
                                        damageWriteNoted = damageTracker.writeState;
                                        game_prompts::note(damageWriteNoted == 2
                                                               ? "VERSION 0.0.5 DAMAGE TEST: the mod's damage CONFIRMED (HP fell, no X)"
                                                               : "VERSION 0.0.5 DAMAGE TEST: first blow sent (CANDIDATE)");
                                    }
                                    publish(SpidyV005Data, [&](v005::Data& d) {
                                        d.bytes = sizeof(v005::Data);
                                        d.flags = 1u;
                                        d.heroHeightValue = c38(combat38::heroHeight);
                                        d.heroDamageValue = c38(combat38::heroDamage);
                                        d.heroHeightK = heroHeightNow.load(std::memory_order_relaxed);
                                        d.sfxState = sfxNow.state;
                                        d.sfxCode = sfxNow.code;
                                        d.sfxAsked = sfxNow.asked;
                                        d.sfxPlayed = sfxNow.played;
                                        d.sfxDropped = sfxNow.dropped;
                                        d.sfxMuted = sfxNow.muted;
                                        d.sfxGain = sfxNow.gain;
                                        d.punchSenseValue = c38(combat38::punchSense);
                                        d.sfxVolumeValue = c38(combat38::sfxVolume);
                                        d.supersonicValue = c38(combat38::supersonic);
                                        d.supersonicSpeedValue = c38(combat38::supersonicSpeed);
                                        d.supersonicEngages = flyBoost.engages();
                                        d.flySpeedNow = flyNow.flying && finite(swingState.velocity) ? length(swingState.velocity) : 0.f;
                                        const auto& t = damageTracker;
                                        d.healthState = t.healthState;
                                        d.writeState = t.writeState;
                                        d.enemiesNow = vitalCount;
                                        d.enemiesWithHealth = withHealth;
                                        d.healthComponents = world_trace::healthComponents();
                                        d.samples = t.samples;
                                        d.enemiesRead = t.enemiesRead;
                                        d.falls = t.falls;
                                        d.fallsMelee = t.fallsMelee;
                                        d.fallsOurs = t.fallsOurs;
                                        d.fallsUnexplained = t.fallsUnexplained;
                                        d.knockouts = t.knockouts;
                                        d.rises = t.rises;
                                        d.biggestFall = t.biggestFall;
                                        d.nearestHp = vitalCount && vitals[0].have ? vitals[0].hp : -1.f;
                                        d.nearestMost = vitalCount && vitals[0].have ? vitals[0].most : -1.f;
                                        d.meleePresses = ds.meleePresses;
                                        const uint32_t first = t.nextEvent > damage_probe::events ? t.nextEvent - damage_probe::events : 0;
                                        for (uint32_t e = first, k = 0; e < t.nextEvent; ++e, ++k)
                                            d.recent[k] = t.recent[e % damage_probe::events];
                                        d.writeStatus = ds.status;
                                        d.writeCode = ds.code;
                                        d.queued = ds.queued;
                                        d.sent = ds.sent;
                                        d.notSlot = ds.notSlot;
                                        d.full = ds.full;
                                        d.faults = ds.faults;
                                        d.refusedOff = ds.refusedOff;
                                        d.lastRecord = ds.lastRecord;
                                        d.lastHandle = ds.lastHandle;
                                    });
                                    publish(SpidyV006Data, [&](v006::Data& d) {
                                        d.bytes = sizeof(v006::Data);
                                        d.flags = 1u;
                                        d.daredevilValue = c38(combat38::daredevil);
                                        d.hulkValue = c38(combat38::hulk);
                                        d.daredevilPunchValue = c38(combat38::daredevilPunch);
                                        d.heroNow = heroWas;
                                        d.batonThrows = d.batonHits = d.batonCatches = d.batonRefused = 0;
                                        for (const auto& b : batons) {
                                            d.batonThrows += b.throws;
                                            d.batonHits += b.hits;
                                            d.batonCatches += b.catches;
                                            d.batonRefused += b.refused;
                                        }
                                        d.batonBlows = batonBlowsSent;
                                        d.ddPunchBlows = ddPunchBlows;
                                        d.hulkPunchBlows = hulkPunchBlows;
                                        d.punchMisses = heroPunchMisses;
                                        d.ironBlows = repBlowsSent;
                                        d.sonarState = heroWas != hero_modes::daredevil ? 0u
                                                       : (native_gpu::eyeToonFault() || native_gpu::eyeSharpenFault()) ? 2u : 1u;
                                        d.sonicLast = sonicLast;
                                        d.sonicBest = sonicBest;
                                    });
                                    // Version 0.0.62: its new parts' counts in the report, every 20 s when one moved.
                                    const uint64_t v0062Sum = instantBlows + instantMisses + batonSwingHits + wallAnswers +
                                                              beamClipped + beamUnclipped + hitMarks.added();
                                    if (v0062Sum != v0062Noted && combatClock - v0062NotedAt >= 20) {
                                        v0062Noted = v0062Sum;
                                        v0062NotedAt = combatClock;
                                        uint64_t walledShots = oursBalls.walled();
                                        char line[400];
                                        sprintf_s(line,
                                                  "VERSION 0.0.62 COMBAT: instant punch blows %llu (no enemy %llu), baton swing hits %llu; "
                                                  "enemy tracking live %llu / 0.0.6 %llu frames (%.0f a second); bolt walls: casts %llu, answers %llu, "
                                                  "walls %llu, shots stopped %llu, beams clipped %llu / clear %llu; hit marks %llu",
                                                  static_cast<unsigned long long>(instantBlows), static_cast<unsigned long long>(instantMisses),
                                                  static_cast<unsigned long long>(batonSwingHits), static_cast<unsigned long long>(foesLiveFrames),
                                                  static_cast<unsigned long long>(foesOldFrames), static_cast<double>(world_trace::enemyScanHz()),
                                                  static_cast<unsigned long long>(wallCasts), static_cast<unsigned long long>(wallAnswers),
                                                  static_cast<unsigned long long>(wallBlocked), static_cast<unsigned long long>(walledShots),
                                                  static_cast<unsigned long long>(beamClipped), static_cast<unsigned long long>(beamUnclipped),
                                                  static_cast<unsigned long long>(hitMarks.added()));
                                        game_prompts::note(line);
                                    }
                                }
                            }
                            {   // Version 0.0.3: the SUIT ID WATCH (hero instance + 0x8c, CANDIDATE, read-only, 4 a
                                // second): a held change makes the mirror learn the suit's colours afresh.
                                suitWatchClock += std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                                if (suitWatchClock >= .25f) {
                                    suitWatchClock = 0;
                                    // Version 0.0.4 MIRROR VIEW TEST: the wish, and how it went.
                                    const uint32_t mirrorWant = c38(combat38::mirrorView);
                                    SpidyMirrorViewWant(mirrorWant);
                                    SpidyMirrorViewRead(&mirrorStats);
                                    const bool late = mirrorWant && mirrorStats.tooLate;
                                    if (late && !mirrorTooLate)
                                        game_prompts::note("VERSION 0.0.4 MIRROR VIEW TEST: asked after the eyes were made; "
                                                           "restart the game to run it");
                                    mirrorTooLate = late;
                                    // MIRROR TEST (private): MIRROR VIEW stays while ON (OFF on the wrist closes it).
                                    // When it stopped by itself (no slots, no view, too slow, no picture), it
                                    // switches back to OFF and the launcher saves OFF (SpidyHudSettings 3 bit 21,
                                    // as a press does), so a failing mirror does not come back at every start.
                                    if (mirrorWant)
                                        mirrorOffWhy = 0; // ON again: the old reason goes
                                    if (mirrorWant && mirror_view::finished(mirrorStats.test)) {
                                        combat38Values[combat38::mirrorView] = 0;
                                        SpidyMirrorViewWant(0);
                                        wristChanged |= 1u << 21;
                                        const auto& t = mirrorStats.test;
                                        mirrorOffWhy = t.why < mirror_view::whys ? t.why : 0;
                                        mirrorOffNoteUntil = wristClock + 20;
                                        game_prompts::note((std::string("MIRROR TEST: turned itself OFF (saved): ") +
                                                            mirror_view::whyNames[mirrorOffWhy])
                                                               .c_str());
                                        char line[128];
                                        sprintf_s(line, "MIRROR TEST OFF at %.0fs: %.1fms avg (pre-VR %.1f), copies %llu fault %u, hero %llu",
                                                  t.seconds, t.testMs, t.baseMs,
                                                  static_cast<unsigned long long>(mirrorStats.gpuCopies),
                                                  mirrorStats.gpuFault,
                                                  static_cast<unsigned long long>(mirrorStats.heroDraws));
                                        game_prompts::note(line);
                                    }
                                    // MIRROR TEST: how it is going, at 2 s, 15 s, 60 s, then every 2 min (the
                                    // report's timeline keeps 32 lines: not more often).
                                    if (mirrorStats.test.phase == mirror_view::running) {
                                        const float at = mirrorStats.test.seconds;
                                        const bool due = (mirrorLogClock < 2 && at >= 2) || (mirrorLogClock < 15 && at >= 15) ||
                                                         (mirrorLogClock < 60 && at >= 60) ||
                                                         (at >= 60 && at >= mirrorLogClock + 120);
                                        if (due) {
                                            mirrorLogClock = at;
                                            const auto& t = mirrorStats.test;
                                            char line[128];
                                            sprintf_s(line, "MIRROR TEST %.0fs: %.1fms avg, copies %llu fault %u, posed %llu, hero %llu other %llu",
                                                      at, t.testMs, static_cast<unsigned long long>(mirrorStats.gpuCopies),
                                                      mirrorStats.gpuFault, static_cast<unsigned long long>(mirrorStats.posed),
                                                      static_cast<unsigned long long>(mirrorStats.heroDraws),
                                                      static_cast<unsigned long long>(mirrorStats.otherDraws));
                                            game_prompts::note(line);
                                        }
                                    } else {
                                        mirrorLogClock = 0;
                                    }
                                    if (player.hero != suitHeroWas) { // another hero: start the watch again
                                        suitWatch.forget();
                                        suitHeroWas = player.hero;
                                    }
                                    uint64_t instance{};
                                    uint32_t suit{};
                                    const bool ok = player.record && read(static_cast<uintptr_t>(player.record), &instance, 8) &&
                                                    instance >= 0x10000 &&
                                                    read(static_cast<uintptr_t>(instance + mirror_view::suitIdAt), &suit, 4);
                                    if (suitWatch.step(ok, suit)) {
                                        suitColours.forget();
                                        ++suitForgot;
                                        char line[128];
                                        sprintf_s(line, "Version 0.0.3: SUIT ID WATCH: the suit id is now 0x%x (CANDIDATE): "
                                                        "the mirror learns its colours again", mirror_view::suitPart(suit));
                                        game_prompts::note(line);
                                    }
                                }
                            }
                            {   // Version 0.0.3: the V003 record (tools/update37_records.py 'v003').
                                PerfReport perf;
                                {
                                    std::lock_guard<std::mutex> lock(perfReportLock);
                                    perf = perfReport;
                                }
                                static uint32_t perfEventsNoted = 0;
                                if (perf.eventCount > perfEventsNoted) { // AUTO PERFORMANCE: each adjustment logged
                                    const uint32_t first = perf.eventCount > v003::perfEvents ? perf.eventCount - v003::perfEvents : 0;
                                    for (uint32_t e = std::max(first, perfEventsNoted); e < perf.eventCount; ++e) {
                                        const auto& ev = perf.log[e - first];
                                        char line[160];
                                        sprintf_s(line, "Version 0.0.3: AUTO PERF %s -> %s (%s, %.1f FPS)",
                                                  auto_perf::rungNames[ev.from % auto_perf::rungs],
                                                  auto_perf::rungNames[ev.to % auto_perf::rungs],
                                                  auto_perf::whyNames[ev.why % auto_perf::whys], ev.fps);
                                        game_prompts::note(line);
                                    }
                                }
                                perfEventsNoted = perf.eventCount;
                                publish(SpidyV003Data, [&](v003::Data& d) {
                                    d.bytes = sizeof(v003::Data);
                                    d.flags = 1u;
                                    for (uint32_t o = 0; o < v003::newOptions; ++o)
                                        d.values[o] = c38(combat38::ropeGrab + o);
                                    d.perfOn = perf.on;
                                    d.perfRung = perf.rung;
                                    d.perfZone = perf.zone;
                                    d.perfEventCount = perf.eventCount;
                                    d.perfTarget = perf.target;
                                    d.perfMeanFps = perf.meanFps;
                                    d.perfMissed = perf.missed;
                                    d.perfHeadsetHz = perf.headsetHz;
                                    d.userFarPercent = perf.userFar;
                                    d.userSharedLod = perf.userLod;
                                    d.userFarShadows = perf.userShadows;
                                    d.nowFarPercent = perf.nowFar;
                                    d.nowSharedLod = perf.nowLod;
                                    d.nowFarShadows = perf.nowShadows;
                                    d.applyRefused = perf.refused;
                                    d.userChangedWhileOn = perf.userChanged;
                                    d.perfDrops = perf.drops;
                                    d.perfRaises = perf.raises;
                                    d.perfZoneJumps = perf.zoneJumps;
                                    d.perfBans = perf.bans;
                                    d.perfRestores = perf.restores;
                                    for (uint32_t e = 0; e < v003::perfEvents; ++e)
                                        d.perfLog[e] = perf.log[e];
                                    for (unsigned i = 0; i < 2; ++i) {
                                        d.tetherOn[i] = ropes[i].on;
                                        d.tetherWhy[i] = ropes[i].why;
                                        d.tetherId[i] = ropes[i].on ? grabIds[i] : 0;
                                        d.tetherLength[i] = ropes[i].length;
                                        d.tetherSpeed[i] = length(ropes[i].on ? ropes[i].velocity : ropes[i].releaseVelocity);
                                    }
                                    d.ropeAttaches = ropes[0].attaches + ropes[1].attaches;
                                    d.ropeYanks = ropeYanksSent;
                                    d.ropeClaps = ropeClaps;
                                    d.ropeNoTarget = ropeNoTarget;
                                    d.ropeBothOnOne = ropeBothOnOne;
                                    for (uint32_t w = 0; w < rope_grab::releases && w < 8; ++w)
                                        d.ropeReleases[w] = ropes[0].releasesBy[w] + ropes[1].releasesBy[w];
                                    d.wallImpacts = wallStick.impacts();
                                    d.wallShown = wallStick.shown();
                                    d.wallWalls = wallStick.walls();
                                    d.wallFloors = wallStick.floors();
                                    d.wallUnstuck = wallStick.unstuck();
                                    d.wallArms = wallArms;
                                    uint32_t netsNow = 0;
                                    for (const auto& n : wallStick.netsNow())
                                        netsNow += n.on ? 1u : 0u;
                                    d.wallNetsNow = netsNow;
                                    d.suitId = suitWatch.now;
                                    d.suitHave = suitWatch.have;
                                    d.suitReads = suitWatch.reads;
                                    d.suitFailed = suitWatch.failed;
                                    d.suitChanges = suitWatch.changes;
                                    d.suitColoursForgot = suitForgot;
                                    const auto& mt = mirrorStats.test; // Version 0.0.4 MIRROR VIEW TEST
                                    d.testPhase = mt.phase;
                                    d.testWhy = mt.why;
                                    d.testSlot = mt.slot;
                                    d.testRuns = mt.runs;
                                    d.testSeconds = mt.seconds;
                                    d.testBaseMs = mt.baseMs;
                                    d.testMs = mt.testMs;
                                    d.testFrames = mt.frames;
                                    d.testWidth = mirror_view::width;
                                    d.testHeight = mirror_view::height;
                                    d.testTexture = mirrorStats.texture;
                                    d.testFreeSlots = mirrorStats.freeSlots;
                                    d.testHeroDraws = mirrorStats.heroDraws;
                                    d.testOtherDraws = mirrorStats.otherDraws;
                                    d.botOn = ropeWatched != 0;
                                    world_trace::botProbe(d.bot);
                                });
                            }
                        }
                        {   // Update 37.2: SLOT TEST / WEB BALL TEST, sent 1.5 s after the press and logged.
                            static combat_input::SlotTest slotTester, ballTester;
                            switch (testAsked.exchange(0)) {
                            case 1:
                                slotTester.press(frame.seconds);
                                break;
                            case 2:
                                ballTester.armStep(static_cast<int>(combat_input::testCount) - 1, frame.seconds); // RB TAP
                                break;
                            default:
                                break;
                            }
                            bool slotSent{}, ballSent{};
                            pad.buttons |= slotTester.update(frame.seconds, slotSent);
                            pad.buttons |= ballTester.update(frame.seconds, ballSent);
                            if (slotSent) {
                                char line[96];
                                sprintf_s(line, "Update 37.2: SLOT TEST sent %s (test %llu)", slotTester.name(),
                                          static_cast<unsigned long long>(slotTester.sends));
                                game_prompts::note(line);
                            }
                            if (ballSent) {
                                char line[96];
                                const uint32_t bloom = SpidyEyeBloomNow() & 0xff;
                                sprintf_s(line, "Update 37.2: WEB BALL TEST shot, EYE BLOOM %s",
                                          eye_bloom::modeName(bloom));
                                game_prompts::note(line);
                            }
                            slotTestPacked = slotTester.pack(frame.seconds);
                            webBallPacked = ballTester.pack(frame.seconds);
                        }
                        // Update 38.5 GESTURE MOVES ON: a recognised move's presses, and the wings' steering
                        // while the stick is let go (the stick always wins).
                        if (gesturePlayNow && gestureMovesNow.load() == gestures385::modeOn) {
                            pad.buttons = static_cast<uint16_t>(pad.buttons | comboSender.update(frame.seconds));
                            const bool stickHeld = std::abs(static_cast<int>(pad.thumbLX)) > 8000 ||
                                                   std::abs(static_cast<int>(pad.thumbLY)) > 8000;
                            if (gestureOut.steering && !stickHeld) {
                                pad.thumbLX = static_cast<int16_t>(std::clamp(gestureOut.stickX, -1.f, 1.f) * 32767);
                                pad.thumbLY = static_cast<int16_t>(std::clamp(gestureOut.stickY, -1.f, 1.f) * 32767);
                            }
                        } else {
                            comboSender.clear();
                        }
                        // Version 0.0.4 HERO ABILITIES: no swing, zip, charged swing leap, web shooter,
                        // gadget or yank reaches the game for a hero with no webs (a story prompt's ask kept).
                        if (mapping == game_pad::Mapping::gameplay) {
                            static uint64_t heroWebsTaken = 0;
                            static double heroNotedAt = -1e9;
                            // Version 0.0.7: BATMAN loses RT too (his grapple gun is cape_glide.hpp's, not the game's web zip).
                            const uint32_t taken = hero_modes::stripPad(heroNow(), pad.buttons, pad.rightTrigger,
                                                                        walk.demand.buttons, walk.demand.rightTrigger,
                                                                        pad.leftTrigger);
                            heroWebsTaken += taken;
                            if (taken && frame.seconds - heroNotedAt > 10) {
                                heroNotedAt = frame.seconds;
                                char line[128];
                                sprintf_s(line, "VERSION 0.0.4 IRON MAN ABILITIES: %llu web inputs kept from the game "
                                                "(swing, zip, web shooter, gadgets)",
                                          static_cast<unsigned long long>(heroWebsTaken));
                                game_prompts::note(line);
                            }
                        }
                        game_pad::submit(pad, 200);
                        // Update 38.5 Y IN THE AIR: read-only counts (no glide state is read; air_watch.hpp).
                        {
                            const bool yNow = (pad.buttons & game_pad::y) != 0;
                            if (yNow && !ySending) {
                                ++ySent;
                                yInAir += inFlight;
                            }
                            ySending = yNow;
                            if (sampleAirYield && !sampleAirYield(&airYieldState) && airYieldState.magic == 0x444c5941 &&
                                airYieldState.version == 1 && airYieldState.bytes == sizeof(airYieldState)) {
                                if (airYieldState.outlived > outlivedNoted) {
                                    outlivedNoted = airYieldState.outlived;
                                    game_prompts::note("Update 38.5 Y IN THE AIR: the game's own move kept the hero past 3 s "
                                                       "(a glide's sign, CANDIDATE); the swing stays off until a trigger");
                                }
                                if (airYieldState.yields > yieldsNoted) {
                                    yieldsNoted = airYieldState.yields;
                                    char line[128];
                                    sprintf_s(line, "Update 38.5 Y IN THE AIR: the game took the hero out of the swing's flight (#%llu)",
                                              static_cast<unsigned long long>(yieldsNoted));
                                    game_prompts::note(line);
                                }
                            }
                            spidy::air_watch::Data aw;
                            aw.yielding = airYieldState.yielding;
                            aw.ySent = ySent;
                            aw.yInAir = yInAir;
                            aw.yankInputs = grabYankInputs;
                            aw.yanksInAir = yanksInAir;
                            aw.yields = airYieldState.yields;
                            aw.outlived = airYieldState.outlived;
                            aw.takenBack = airYieldState.takenBack;
                            aw.capped = airYieldState.capped;
                            aw.longestMs = airYieldState.longestMs;
                            aw.lastMs = airYieldState.lastMs;
                            InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&SpidyAirWatchData.sequence));
                            const uint64_t sequence = SpidyAirWatchData.sequence;
                            aw.sequence = sequence;
                            SpidyAirWatchData = aw;
                            SpidyAirWatchData.sequence = sequence + 1;
                        }
                        if (walk.interact && !interacting) {
                            ++interacts;
                            wallInteracts += onWall;
                            char line[96];
                            sprintf_s(line, "the player's Y -> the game%s%s", onWall ? ", on a wall" : "",
                                      walk.demand.any() ? ", carrying a prompt's demand" : "");
                            game_prompts::note(line);
                        }
                        interacting = walk.interact;
                        dodges += walk.dodge && !dodging;
                        dodging = walk.dodge;
                        heals += walk.heal && !healing;
                        healing = walk.heal;
                        zips += walk.zip && !zipping;
                        zipping = walk.zip;
                    }
                    perfSections.mark(perf37::swing); // Update 37: where the CPU time goes
                    // Body haptics: what goes on, every frame, in VR play only (a
                    // menu, a scene, the game screen: all quiet).
                    {
                        body_haptics::State felt;
                        felt.live = immersive;
                        felt.groups = hapticGroups();
                        // The room's turn into the world: the world's head over the room's.
                        if (validTrackedPose(frame.head) && validTrackedPose(motion.headPose))
                            torso = facing.update(frame.head.orientation,
                                                  motion.headPose.orientation * frame.head.orientation.conjugate(),
                                                  frame.seconds);
                        const bool fresh = swingStarted && swingState.status && swingState.qpc &&
                                           static_cast<int64_t>(swingState.qpc) > now.QuadPart - frequency.QuadPart / 10 &&
                                           finite(swingState.velocity);
                        felt.airborne = fresh && game_swing::airborne(swingState, now.QuadPart, frequency.QuadPart);
                        felt.onWall = fresh && swingState.wall != 0;
                        if (fresh)
                            felt.velocity = torso.local(swingState.velocity);
                        for (unsigned i = 0; i < 2; ++i) {
                            const bool grabbing = grabPhases[i] != 0;
                            felt.webTension[i] = fresh && attachedBefore[i] && !grabbing ? swingState.webs[i].tension : 0.f;
                            felt.grabTension[i] = grabbing ? grabState.hands[i].tension : 0.f;
                        }
                        felt.charge = chargeJump.charging() ? chargeJump.charge() : 0.f;
                        felt.slowMotion = slowMotion.active();
                        // A fall in the hero's hit points is a blow: from the attack
                        // the game warned of last, within a second, else on the chest
                        // and the back at once.
                        float hitPoints = std::numeric_limits<float>::quiet_NaN();
                        float mostHitPoints = std::numeric_limits<float>::quiet_NaN();
                        if (const auto h = players.health(); h && player.hero) {
                            read(h + game_player::hitPointsAt, &hitPoints, sizeof(hitPoints));
                            read(h + game_player::maxHitPointsAt, &mostHitPoints, sizeof(mostHitPoints));
                            // Update 38.5 INVINCIBLE: only into the CONFIRMED HeroHealth record (its
                            // vtable checked now), never above the game's own cap, never a revive.
                            uint64_t table{};
                            float reduce = std::numeric_limits<float>::quiet_NaN();
                            const bool record = read(h, &table, sizeof(table)) &&
                                                table == config.base + extras385::invincible::healthTable;
                            invincibleNa = record ? 0u : 1u;
                            if (invincibleOn.load() && record) {
                                read(h + extras385::invincible::reduceAt, &reduce, sizeof(reduce));
                                const float to = extras385::invincible::refill(hitPoints, mostHitPoints, reduce);
                                if (to >= 0 && writeFloatIf(h + game_player::hitPointsAt, hitPoints, to)) {
                                    ++invincibleRefills;
                                    hitPoints = to;
                                }
                            } else if (record && hero072KeepTopped) {
                                // Version 0.0.72 NATURAL INVINCIBLE (THOR, SUPERMAN): INVINCIBLE's refill, through
                                // the same CONFIRMED record and cap (never above it, never a revive).
                                read(h + extras385::invincible::reduceAt, &reduce, sizeof(reduce));
                                const float to = extras385::invincible::refill(hitPoints, mostHitPoints, reduce);
                                if (to >= 0 && writeFloatIf(h + game_player::hitPointsAt, hitPoints, to)) {
                                    ++hero072Refills;
                                    hitPoints = to;
                                }
                            } else if (record && heroNow() == hero_modes::wolverine) {
                                // Version 0.0.7 WOLVERINE HEALING: 3 s unhurt, then 10%/s back, through the same
                                // CONFIRMED record and cap as INVINCIBLE (never above it, never a revive).
                                read(h + extras385::invincible::reduceAt, &reduce, sizeof(reduce));
                                const float healDt = healAt < 0 ? 0.f : static_cast<float>(std::clamp(frame.seconds - healAt, 0.0, .1));
                                const float to = hero007Play.healing.step(healDt, hitPoints, mostHitPoints, reduce);
                                if (to >= 0 && writeFloatIf(h + game_player::hitPointsAt, hitPoints, to)) {
                                    ++hero007Heals;
                                    hitPoints = to;
                                }
                            } else if (record && heroNow() == hero_modes::captainAmerica) {
                                // Version 0.0.7 CAPTAIN AMERICA SHIELD BLOCK (EXPERIMENTAL, OFF by default): a
                                // fall in health while the shield on the arm faces the nearest enemy (a
                                // CANDIDATE guess at "the hit met the shield") is given back through the same
                                // CONFIRMED record and cap as INVINCIBLE: never above it, never a revive.
                                read(h + extras385::invincible::reduceAt, &reduce, sizeof(reduce));
                                const float to = hero007Play.block.step(hitPoints, mostHitPoints, reduce,
                                                                        captain::blockOn(c38(combat38::shieldBlock)),
                                                                        hero007Play.guarding, invincibleOn.load());
                                if (to >= 0 && writeFloatIf(h + game_player::hitPointsAt, hitPoints, to))
                                    hitPoints = to;
                            }
                            if (!record || heroNow() != hero_modes::captainAmerica)
                                hero007Play.block.hpWas = -1; // a stale reading never refunds later
                            healAt = frame.seconds;
                        } else {
                            invincibleNa = 1u;
                        }
                        heroShare = health_ring::healthShare(hitPoints, mostHitPoints);
                        // Version 0.0.72: the share lost since the last hero072 step (SURVIVAL, RAGE, rep).
                        hero072LostKnown = std::isfinite(heroShare);
                        if (hero072LostKnown && std::isfinite(hero072ShareWas) && heroShare < hero072ShareWas &&
                            heroShare > 0)
                            hero072Lost += hero072ShareWas - heroShare;
                        hero072ShareWas = hero072LostKnown ? heroShare : std::numeric_limits<float>::quiet_NaN();
                        // Update 38 WALL TILT: the share reaching 0 (a death) or rising from it
                        // (the respawn) levels the view.
                        if (std::isfinite(heroShare)) {
                            if (std::isfinite(heroShareWas) && (heroShareWas > 0) != (heroShare > 0))
                                rig.diedOrRespawned();
                            heroShareWas = heroShare;
                        }
                        // Update 37: the health read logged each second and around blows
                        // and heals (health_log.hpp), so a session confirms the CANDIDATE.
                        {
                            const bool found = players.health() && player.hero;
                            const uint64_t falls = SpidyHealthData.falls, rises = SpidyHealthData.rises;
                            const uint32_t status = SpidyHealthData.status;
                            publish(SpidyHealthData, [&](health_log::Data& d) {
                                healthLog.update(d, frame.seconds, found, hitPoints, mostHitPoints, u37.healPressed);
                            });
                            u37.healPressed = false;
                            if (SpidyHealthData.falls != falls || SpidyHealthData.rises != rises ||
                                SpidyHealthData.status != status) {
                                char line[128];
                                sprintf_s(line, "Update 37: health %.1f / %.1f (%s)", SpidyHealthData.hp,
                                          SpidyHealthData.most,
                                          SpidyHealthData.status == 3   ? "confirmed: it changed"
                                          : SpidyHealthData.status == 2 ? "valid"
                                          : SpidyHealthData.status == 1 ? "invalid: the ring shows N/A"
                                                                        : "not found");
                                game_prompts::note(line);
                            }
                        }
                        const float lost = heroHealth.update(hitPoints);
                        felt.health = heroHealth.fraction();
                        if (lost > 0 && immersive) {
                            ++hits;
                            const float strength = std::clamp(lost * 4, 0.f, 1.f);
                            const Vec3 to = blowFrom - (motion.headPose.position - Vec3{0, .4f, 0});
                            if (blowSeen && frame.predictedDisplayTime - blowSeen < 1'000'000'000 && length(to) > .1f) {
                                ++aimedHits;
                                feelFrom(body_haptics::Kind::hit, -1, strength, to);
                            } else {
                                feel(body_haptics::Kind::hit, -1, strength);
                            }
                        }
                        const float landed =
                            landingWatch.update(frame.seconds, felt.airborne, fresh ? -swingState.velocity.y : 0.f);
                        if (landed > 0 && immersive) {
                            ++landings;
                            feel(body_haptics::Kind::landing, -1, landed);
                        }
                        haptics.set(felt);
                        // The player hears once what the devices do: on, and which; or
                        // why not. Without a Player installed (most players), nothing.
                        const auto told = haptics.counters();
                        if (told.status != hapticsTold ||
                            (told.status == bhaptics::Status::playing && told.devices != devicesTold)) {
                            std::string text;
                            switch (told.status) {
                            case bhaptics::Status::playing:
                                text = "Body haptics on: " + bhaptics::deviceNames(told.devices);
                                break;
                            case bhaptics::Status::connected:
                                text = "Body haptics: the bHaptics Player has no device on - pair them in it";
                                break;
                            case bhaptics::Status::noLibrary:
                                text = "Body haptics: bhaptics_library.dll is missing beside Spidy's modules";
                                break;
                            case bhaptics::Status::noApp:
                                text = "Body haptics: this build of Spidy has no bHaptics app";
                                break;
                            case bhaptics::Status::failed:
                                text = "Body haptics: the bHaptics SDK would not start";
                                break;
                            case bhaptics::Status::rejected:
                                text = "Body haptics: the bHaptics Player did not take this build's bHaptics app";
                                break;
                            default:
                                break;
                            }
                            if (!text.empty())
                                message(3, 0, text.c_str());
                            hapticsTold = told.status;
                            devicesTold = told.devices;
                        }
                        // Installed but not answering for ten seconds: not running.
                        if (told.status == bhaptics::Status::waiting && hapticsSince &&
                            GetTickCount64() - hapticsSince > 10000) {
                            hapticsSince = 0;
                            message(3, 0, "Body haptics: start the bHaptics Player to feel webs, blows and "
                                          "Spider-Sense on your vest - it connects by itself");
                        }
                    }
                    {
                        std::lock_guard lock(telemetry);
                        auto& d = SpidyXrData;
                        InterlockedIncrement64(&d.sequence);
                        ++d.frames;
                        d.tracked += motion.active;
                        d.leftHands += frame.hands[0].valid;
                        d.rightHands += frame.hands[1].valid;
                        d.nativeKeys = keys;
                        d.gate = gate;
                        d.cameraMover = moverTable > config.base && moverTable - config.base < 0x10000000
                                            ? static_cast<uint32_t>(moverTable - config.base)
                                            : 0;
                        d.players = playersFound;
                        d.playerRecord = player.record;
                        const auto pad = game_pad::telemetry();
                        d.padButtons = pad.buttons;
                        d.padInstalled = pad.installed;
                        d.padReads = pad.reads;
                        d.cameraCommits = game.cameraCalls;
                        d.playerCommits = game.matched;
                        d.interacts = interacts;
                        d.dodges = dodges;
                        d.heals = heals;
                        d.zips = zips;
                        d.zipReleases = zipPress.releases();
                        d.aimMarkers = values.aimMarkers;
                        d.settings = (values.webGrab ? 1u : 0u) | (values.punch ? 2u : 0u) | (values.body ? 4u : 0u) |
                                     (values.airWebs ? 8u : 0u) | (values.webShooter ? 16u : 0u) |
                                     (values.flips ? 32u : 0u) | (values.triggerWebs ? 64u : 0u) |
                                     (values.standOnWalls ? 128u : 0u) | (values.arcadeWebs ? 512u : 0u);
                        d.scans = templePress.scans();
                        d.chargeJumps = chargeJump.charges();
                        const auto prompts = game_prompts::live();
                        d.promptInvokes = prompts.invokes;
                        d.promptCalls = prompts.calls;
                        d.promptsRaised = prompts.raised;
                        d.promptsCleared = prompts.cleared;
                        d.promptTexts = prompts.named;
                        d.promptRewrites = prompts.rewritten;
                        d.promptDemand = prompts.demand;
                        d.promptLastDemand = prompts.lastDemand;
                        d.promptBeyondPlay = prompts.beyondPlay;
                        d.promptKeyboardTexts = prompts.keyboardTexts;
                        d.contextPresses = namedPress.presses();
                        d.padEntries = padLayer.entries();
                        d.padLayerNow = padLayer.latched() ? 1u : 0u;
                        d.wallInteracts = wallInteracts;
                        d.wallHandovers = wallHandover.handovers();
                        d.wallHandoversOffered = wallHandover.offered();
                        d.headPushFrames = rig.clearance().pushedFrames();
                        d.headPushMostMm = static_cast<uint32_t>(std::lround(rig.clearance().most() * 1000));
                        d.crawlFrames = crawlFrames;
                        d.crawlStood = crawlStood;
                        d.crawlStickSamples = crawlStick.samples();
                        d.crawlStickTurn = crawlStick.turn();
                        d.crawlStickAgreement = crawlStick.agreement();
                        d.crawlStickSpeed = crawlStick.speed();
                        d.headDroppedFrames = headDroppedFrames;
                        d.headroomLeast = headroomLeast;
                        if (sampleWarp && !sampleWarp(&warp)) {
                            d.crawlAsked = static_cast<uint32_t>(warp.asked);
                            d.crawlApplied = static_cast<uint32_t>(warp.applied);
                            d.crawlRefused = static_cast<uint32_t>(warp.refused);
                            d.crawlFaulted = static_cast<uint32_t>(warp.faulted);
                            d.crawlTaken = static_cast<uint32_t>(warp.taken);
                            d.crawlDeclined = static_cast<uint32_t>(warp.declined);
                            gameRunFrames += (warp.swing & 32u) != 0;
                        }
                        d.gameRunFrames = gameRunFrames;
                        d.holdGraceFrames = holdGraceFrames;
                        d.trackingGapFrames = trackingGapFrames;
                        d.trackingGapReasons = trackingGapReasons;
                        d.floorCrawlFrames = floorCrawlFrames;
                        d.crawlCapFrames = crawlCapFrames;
                        d.kneelFrames = kneelFrames;
                        d.offerPointFrames = offerPointFrames;
                        d.offerReachFrames = offerReachFrames;
                        d.offerAsks = offerUse.asks();
                        d.offerRefusals = offerUse.refusals();
                        d.offerPresses = offerUse.presses();
                        d.offerPicked = offerUse.picked();
                        d.offerFailures = offerUse.failures();
                        d.offersSeen = offersSeen;
                        d.offersState = offers.status | offers.error << 8;
                        d.offersMasked = static_cast<uint32_t>(offers.masked);
                        d.offersPosed = static_cast<uint32_t>(offers.posed);
                        d.offersGathered = offers.gathered;
                        d.offersMostChecked = offers.mostChecked;
                        d.offersSearches = offers.searches;
                        d.offersSpots = offers.spotsFound;
                        d.offersNear = offers.offers;
                        d.offerRetries = offerUse.retries();
                        d.senseShown = senseShown;
                        d.senseFrames = senseFrames;
                        d.senseSightFrames = senseSightFrames;
                        d.senseTingles = senseTingles;
                        d.senseBroken = senseBroken;
                        d.senseDodges = senseDodges;
                        for (unsigned i = 0; i < 3; ++i) {
                            d.cameraForward[i] = camera[8 + i];
                            d.cameraUp[i] = camera[4 + i];
                        }

                        d.vrButtons = trackedFrame.buttons;
                        d.vrButtonsPlayed = vrButtonsPlayed;
                        auto& p = SpidyPromptData;
                        InterlockedIncrement64(&p.sequence);
                        p.invokes = prompts.invokes;
                        p.calls = prompts.calls;
                        p.raised = prompts.raised;
                        p.cleared = prompts.cleared;
                        p.texts = prompts.named;
                        p.rewrites = prompts.rewritten;
                        p.demand = prompts.demand;
                        p.lastDemand = prompts.lastDemand;
                        p.beyondPlay = prompts.beyondPlay;
                        p.keyboardTexts = prompts.keyboardTexts;
                        // A name can join the census with no prompt call of its own, so
                        // the lists are copied out every so often as well as on a call.
                        if (prompts.calls != promptCallsBefore || ++promptRefresh % 64 == 0) {
                            promptCallsBefore = prompts.calls;
                            game_prompts::Calls seen;
                            game_prompts::recentCalls(seen);
                            p.lines = seen.count;
                            std::memcpy(p.perName, seen.perName, sizeof(p.perName));
                            std::memcpy(p.line, seen.line, sizeof(p.line));
                            p.names = seen.names;
                            std::memcpy(p.censusCalls, seen.calls, sizeof(p.censusCalls));
                            std::memcpy(p.example, seen.example, sizeof(p.example));
                        }
                        InterlockedIncrement64(&p.sequence);
                        {
                            const auto felt = haptics.counters();
                            auto& h = SpidyHapticsData;
                            InterlockedIncrement64(&h.sequence);
                            h.status = static_cast<uint32_t>(felt.status);
                            h.level = static_cast<uint32_t>(values.bodyHaptics);
                            h.devices = felt.devices;
                            h.devicesSeen = felt.devicesSeen;
                            h.connects = felt.connects;
                            h.events = felt.events;
                            h.frames = felt.frames;
                            h.refused = felt.refused;
                            h.hits = hits;
                            h.aimedHits = aimedHits;
                            h.landings = landings;
                            h.liveTicks = felt.liveTicks;
                            std::copy(felt.onTicks.begin(), felt.onTicks.end(), h.onTicks);
                            std::copy(felt.peakSum.begin(), felt.peakSum.end(), h.peakSum);
                            std::copy(felt.kinds.begin(), felt.kinds.end(), h.kinds);
                            h.groups = hapticGroups();
                            InterlockedIncrement64(&h.sequence);
                        }
                        const auto controllers = runtime.controllers();
                        d.controllers[0] = static_cast<uint32_t>(controllers[0]);
                        d.controllers[1] = static_cast<uint32_t>(controllers[1]);
                        d.controllerLayouts = runtime.layoutsTaken();
                        d.snapTurn = static_cast<uint32_t>(values.snapTurn);
                        d.smoothTurn = static_cast<uint32_t>(values.smoothTurn);
                        d.haptics = static_cast<uint32_t>(values.haptics);
                        d.screenSize = static_cast<uint32_t>(values.screenSize);
                        d.swingSpeed = values.swingSpeed;
                        d.weight = static_cast<uint32_t>(values.weight);
                        d.hud = static_cast<uint32_t>(values.hud);
                        d.flipSpeed = static_cast<uint32_t>(values.flipSpeed);
                        d.viewTilt = motion.active ? motion.viewTilt * 180 / 3.14159265f : 0.f;
                        d.interactSteps = interactStep.steps();
                        d.lookError = cameraLook.error();
                        d.lookTurns = cameraLook.turns();
                        d.screenDepth = static_cast<uint32_t>(values.screenDepth);
                        d.settingChanges = settingChanges;
                        const auto menu = game_menu::telemetry();
                        d.menuTabs = menu.tabs;
                        d.menuInstalled = menu.installed;
                        d.menuStatus = menuCode ? menuCode : menu.status;
                        d.surfaceEntries = surfaceEntries;
                        d.surfaceFrames = surfaceFrames;
                        std::memcpy(d.heroUp, body.data() + 4, 12);
                        d.standOff = length(motion.standOff);
                        if (motion.active && motion.onSurface) {
                            d.surfaceHeight = motion.surfaceHeight;
                            d.surfaceClearance = motion.surfaceClearance;
                        }
                        const bool calibrating = calibration.phase() != body_calibration::Phase::idle;
                        d.calibrationPhase = static_cast<uint32_t>(calibration.phase());
                        d.calibrationHint = calibrating ? static_cast<uint32_t>(calibration.hint()) : 0u;
                        d.calibrationProgress = calibrating ? calibration.progress() : 0.f;
                        d.eyeHeightMm = static_cast<uint32_t>(std::lround(calibratedEye * 1000));
                        d.armLengthMm = static_cast<uint32_t>(std::lround(calibratedArm * 1000));
                        d.calibrations = calibrations;
                        d.calibrationSkips = calibrationSkips;
                        d.calibrationFlags = calibrationPrompt ? 0u : 1u;
                        d.calibrationReach[0] = lastCalibration.reach[0];
                        d.calibrationReach[1] = lastCalibration.reach[1];
                        std::memcpy(d.head, motion.head.data(), 64);
                        const Mat4 basis = {1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1};
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto pose = native_view::relativePose(basis, motion.hands[i]);
                            std::memcpy(d.hands[i], pose.data(), 64);
                        }
                        InterlockedIncrement64(&d.sequence);
                    }
                    perfSections.mark(perf37::haptics); // Update 37: where the CPU time goes
                    // Copies of the game's presented frame, for the screen.
                    if (gpuStarted)
                        native_gpu::wantScreen(viewing && (!vrPlay || native_gpu::eyesLost()));
                    showScreen = screen;
                    if (!viewing)
                        return false;
                    if (!gpuStarted) {
                        // From here the headset shows the game: its screen until
                        // gameplay, then the eye views.
                        native_gpu::Config gpu;
                        gpu.pid = config.pid;
                        gpu.base = config.base;
                        gpu.queue = config.queue;
                        gpu.durationMs = moduleDuration;
                        gpu.copyToXr = 2;
                        gpu.captureImages = config.options & 1;
                        // Options 0x400000 (-EyeAA off): the eyes reach the headset as
                        // the game drew them; otherwise through FXAA (native_eye_gpu.hpp).
                        gpu.eyeAA = (config.options & 0x400000) ? 0 : 1;
                        // The headset's eye size: the eyes, supersampled (-Supersample),
                        // are filtered down to it; otherwise they are the same size.
                        native_gpu::setEyeOutput(dimensions[0], dimensions[1]);
                        check(native_gpu::start(&gpu), "Start native GPU bridge");
                        gpuStarted = true;
                        nativeDeadline =
                            config.durationMs ? GetTickCount64() + config.durationMs : UINT64_MAX;
                        message(3, 0, "Headset active");
                        return false; // initialization can exceed one predicted display interval
                    }
                    // Without gameplay the screen shows the game's presented
                    // frame; until it is up, the last image stays. The eye
                    // views get no command and stop rendering meanwhile.
                    if (!vrPlay && !scene)
                        return screen || gameScreen.holding();
                    if (!nativeStarted) {
                        struct NativeConfig {
                            uint32_t magic, version, bytes, pid;
                            uint64_t base;
                            uint32_t duration, flags, width, height;
                        };
                        // Flag 8: while immersive, the game's active view follows the
                        // head, so work the game does only for that view fits the eyes.
                        // Flag 16: each eye culls what its own previous frame hid, as the
                        // game's views do; without it the render thread did 2-4 times the
                        // work and held VR at 40-65 frames a second.
                        // Flag 32: the game's own view, on the head for that work, is not
                        // rendered as well; it cost as much as an eye and only the monitor
                        // showed it, which gets the left eye instead.
                        // Flag 64 (options 0x80000, --eye-lens-distances): the eyes keep the
                        // distances their own wide lenses give, about half the game's.
                        const uint32_t flags = ((config.options & 4) ? 5 : 13) | ((config.options & 16) ? 0 : 16) |
                                               ((config.options & 0x80000) ? 64 : 0);
                        // SM2 port: never flag 32, even if options lack 0x10000 (the
                        // runner already forces it). SM2VR 0.0.27 always renders the game's
                        // own view (its run_game_vr.py forces game_monitor_view): without
                        // it Spider-Man 2 writes a swapchain back buffer that is not the
                        // current one and the device is lost (DXGI_ERROR_ACCESS_DENIED),
                        // so the game freezes or closes (SM2VR 0.0.5).
                        // The eyes' scene at `upscale` percent of their size, with a
                        // jittered lens and the engine's temporal anti-aliasing (bits 1,
                        // 2, 4): taken when the views are made.
                        const bool antialiased = !(config.options & 0x8000);
                        // Bit 8: no targets for screen-space reflections (options
                        // 0x40000: the game has them off, or --no-eye-reflections).
                        const uint32_t reflections = (config.options & 0x40000) ? 8u << 8 : 0;
                        check(SpidyEyeTemporal(reinterpret_cast<void*>(static_cast<uintptr_t>(
                                  (antialiased ? config.upscale | 7u << 8 : 100u) | reflections |
                                  // Update 34: -EngineTaa (options 0x800000): the eyes' buffers
                                  // get the TAA targets (0x10), for ENGINE TAA.
                                  ((config.options & 0x800000) ? 0x10u << 8 : 0u)))),
                              "Eye anti-aliasing");
                        check(SpidyEyeShadows(reinterpret_cast<void*>(
                                  static_cast<uintptr_t>((config.options & 0x20000) ? 0 : 1))),
                              "Eye shadows");
                        check(SpidyMonitorRate(reinterpret_cast<void*>(static_cast<uintptr_t>(config.monitorRate))),
                              "Monitor rate");
                        // -Supersample: the eyes are rendered larger than the headset's
                        // images (multiples of 8, as the render scale's) and filtered down.
                        const auto rendered = native_gpu::supersample() == 100
                                                  ? std::array<uint32_t, 2>{dimensions[0], dimensions[1]}
                                                  : scaledEyeSize(dimensions[0], dimensions[1],
                                                                  native_gpu::supersample());
                        NativeConfig views{
                            0x53534346, 1, 40, config.pid, config.base, moduleDuration, flags, rendered[0],
                            rendered[1]};
                        check(SpidyStart(&views), "Start native views");
                        nativeStarted = true;
                        // Game-drawn webs are optional: without them the overlay draws webs.
                        websStarted = !(config.options & 2) && !native_webs::start(config.base, player.record);
                        // The player's body is optional too: without it the hero stays
                        // hidden and the overlay draws gloves, as before.
                        // GAME BODY on: not before its checks pass (the frame loop starts it).
                        bodyStarted = values.body && c38(combat38::gameBody) != 1 &&
                                      !native_body::start(config.base);
                        firstImageDeadline = GetTickCount64() + 3000;
                        message(3, 0, vrPlay ? "Gameplay found; waiting for the first eye images"
                                             : "A scene found; waiting for its first eye images");
                        // Initialization can exceed one predicted display
                        // interval. A scene's screen keeps the game's frame.
                        return !vrPlay;
                    }
                    if (!vrPlay) {
                        // The scene's two eyes: the game's own camera, set apart
                        // for this viewer and this screen (native_eyes::Stereo).
                        native_eyes::Command eyes;
                        eyes.enabled = 3;
                        eyes.serial = ++serial;
                        eyes.leaseMs = 250;
                        native_eyes::setStereo(
                            eyes, native_eyes::screenStereo(
                                      eyeDistance(frame.eyes[0].pose.position, frame.eyes[1].pose.position),
                                      vr_settings::screenWidth(values.screenSize), vr_settings::screenDistance,
                                      vr_settings::screenDepth(values.screenDepth)));
                        NativeEyeFrame saved;
                        saved.serial = eyes.serial;
                        // A held pair ages by the time it was asked for.
                        saved.motion.predictedDisplayTime = frame.predictedDisplayTime;
                        saved.flatScreen = saved.stereoScreen = true;
                        saved.screenPose = screenPose;
                        saved.screenAspect = SpidyFlatAspect();
                        history.remember(saved);
                        const auto code = SpidySetEyes(&eyes);
                        if (code && code != 4003)
                            check(code, "Scene eye command");
                        stereoScene = !code;
                        return true;
                    }
                    // Spec 3a: up at once, down at most 1 cm/s (body_calibration::followEye, tested).
                    standingEye = body_calibration::followEye(standingEye, frame.head.position.y, frame.seconds);
                    // Version 0.0.72 GAME BODY (EXPERIMENTAL): the strict checks' verdict, logged
                    // when it changes; the writer hooked only once they passed for this player.
                    const bool strictBody = c38(combat38::gameBody) == 1;
                    native_body::strict(strictBody);
                    if (strictBody)
                        native_game_body::begin(config.base); // once; nothing if running
                    else if (native_game_body::running())
                        native_game_body::end();
                    const bool strictVerified = strictBody && native_game_body::verified(player.record);
                    std::array<float, 2> gloveFist{}; // the fallback gloves' fists (glove_overlay)
                    {
                        uint32_t bodySerial{};
                        const auto said = native_game_body::state(bodySerial);
                        const auto refusedRig = bodyStarted && strictBody ? native_body::refused() : std::string{};
                        if (bodySerial != gameBodySerial || refusedRig != gameBodyRigRefusal) {
                            gameBodySerial = bodySerial;
                            gameBodyRigRefusal = refusedRig;
                            const auto line = "GAME BODY: " + said +
                                              (refusedRig.empty() ? std::string{} : "; rig refused: " + refusedRig);
                            message(3, 0, line.c_str());
                            OutputDebugStringA((line + "\n").c_str());
                        }
                        // One try per verdict: a hook that fails waits for the checks to pass anew.
                        if (strictVerified && !bodyStarted && gameBodyTried != bodySerial) {
                            gameBodyTried = bodySerial;
                            const auto f = native_game_body::found();
                            const auto code = native_body::startAt(config.base, f.writer, f.toModel);
                            bodyStarted = !code;
                            const auto line = code ? "GAME BODY: refused: hook " + std::to_string(code)
                                                   : std::string("GAME BODY: hooked; the body is on");
                            message(3, 0, line.c_str());
                            OutputDebugStringA((line + "\n").c_str());
                        }
                    }
                    if (bodyStarted) {
                        // The hero's body follows the headset and the controllers,
                        // placed from the same feet as the eyes.
                        native_body::Command pose;
                        pose.serial = serial;
                        // Under a roof that holds the head below the player's
                        // own: the legs kneel rather than fold up in front of
                        // his eyes.
                        const bool kneels = (motion.standing || crawlsFloor) && motion.headDrop > .05f;
                        kneelFrames += kneels;
                        // Standing on a wall or a ceiling - Spidy's own or the
                        // game's crawl - the body stands on it with the player,
                        // never in the air the swing keeps the hero in there.
                        // MIRROR VIEW (EXPERIMENTAL): while its picture is up (copies made), the head
                        // is not shrunk away, so the mirror shows the whole suit - the game's own hero,
                        // with GAME BODY or BODY on (CANDIDATE: bits of the mask's inside may show at
                        // the edge of the view meanwhile). It shrinks again at once when the picture goes.
                        const bool mirrorShows = c38(combat38::mirrorView) &&
                                                 mirrorStats.test.phase == mirror_view::running &&
                                                 mirrorStats.gpuCopies > 0 && !mirrorStats.gpuFault;
                        // Test build 10 ONE BODY: a worn suit shown in first person replaces the game's body (its arms too).
                        pose.flags = (flatScreen || (strictBody && !strictVerified) ||
                                              !suit_wear::gameBodyDrawn(values.body || strictBody, hero_modes::gameBodyShown(heroNow()),
                                                                        suitShownNow())
                                          ? 0u
                                          : native_body::bodyOn) | // 0.0.72 BATMAN BODY, GAME BODY
                                     (mirrorShows ? 0u : native_body::hideHead) |
                                     native_body::handTurn |
                                     (swingState.owned && !motion.standing ? native_body::airborne : 0u) |
                                     (calibratedEye > 0 ? native_body::calibrated : 0u) |
                                     (motion.standing || crawlsFloor ? native_body::standing : 0u) |
                                     (kneels ? native_body::kneel : 0u);
                        pose.standUp = motion.standUp;
                        pose.floor = motion.floor;
                        pose.eyes = Vec3{(motion.eyes[0][12] + motion.eyes[1][12]) / 2,
                                         (motion.eyes[0][13] + motion.eyes[1][13]) / 2,
                                         (motion.eyes[0][14] + motion.eyes[1][14]) / 2} -
                                    motion.anchor;
                        pose.facing = motion.headPose.orientation;
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& hand = frame.hands[i];
                            const bool tracked = hand.valid && validTrackedPose(hand.grip);
                            const Vec3 relative = hand.grip.position - frame.head.position;
                            const float seconds =
                                std::isfinite(frame.seconds) && frame.seconds > 0 ? frame.seconds : 1.f / 90;
                            const float speed =
                                tracked && gripsSeen[i] ? length(relative - gripsBefore[i]) / seconds : 0.f;
                            gripsBefore[i] = relative;
                            gripsSeen[i] = tracked;
                            HandPoseInput doing;
                            doing.tracked = tracked;
                            doing.wallContact = motion.active && !flatScreen && values.handClimb &&
                                                climbState.status && (climbState.hands & (1u << i));
                            doing.web = motion.swing.hands[i].grip;
                            doing.squeeze = hand.squeeze;
                            doing.speed = speed;
                            doing.gripping = onStrandBefore[i] || webTaken[i];
                            doing.holding = attachedBefore[i];
                            doing.shot = ballShot[i];
                            doing.fighting = fightNear;
                            ballShot[i] = false;
                            const auto fingered = fingers[i].update(seconds, doing);
                            pose.hands[i] = {motion.grips[i].position - motion.anchor, motion.grips[i].orientation,
                                             tracked ? 1u : 0u, fingered.fist};
                            pose.shoot[i] = fingered.shoot;
                            gloveFist[i] = fingered.fist;
                        }
                        pose.height = calibratedEye > 0 ? calibratedEye : standingEye;
                        pose.armLength = calibratedArm;
                        // Version 0.0.5 HERO HEIGHT (hero_height.hpp): a big hero model (a Hulk skin)
                        // stands up straight - the body's eyes, size and arm grow k times; the view,
                        // the hands' places, the swing and FLY MODE do not change. Live; 1.00X: none.
                        {
                            const float heroK = hero_height::factorOf(c38(combat38::heroHeight));
                            const Vec3 bodyUp = motion.standing && finite(motion.standUp) ? motion.standUp
                                                                                          : Vec3{0, 1, 0};
                            const auto grown =
                                hero_height::scaled({pose.eyes, pose.height, pose.armLength}, bodyUp, heroK);
                            pose.eyes = grown.eyes;
                            pose.height = grown.height;
                            pose.armLength = std::min(grown.armLength, 2.f); // native_body's arm check
                            native_body::heroHeight(heroK);
                            heroHeightNow = heroK;
                        }
                        pose.trackingYaw = motion.swing.trackingYaw;
                        // A flip's tilt in world axes: the body turns with the player.
                        // Standing on a surface, the body already stands along it:
                        // only a flip's own turns it further (none on a wall).
                        if (const Quat tilt = motion.bodyTilt();
                            tilt.x != 0 || tilt.y != 0 || tilt.z != 0) {
                            const Quat yaw = Quat::yaw(motion.swing.trackingYaw);
                            pose.tilt = yaw * tilt * yaw.conjugate();
                        }
                        // Update 37.2: WALL BODY lays the body against the wall it stands on.
                        if (motion.standing) {
                            const Quat lay = wall_body::tilt(wallBodyOn.load(std::memory_order_relaxed), motion.standUp);
                            if (lay.x != 0 || lay.y != 0 || lay.z != 0)
                                pose.tilt = lay * pose.tilt;
                        }
                        native_body::submit(pose);
                    }
                    perfSections.mark(perf37::screen); // Update 37: where the CPU time goes
                    // Keep publishing tracking while the engine renders earlier
                    // commands. The presentation loop no longer waits for this pose.
                    native_eyes::Command eyes;
                    eyes.enabled = flatScreen ? 2 : 1;
                    eyes.serial = ++serial;
                    // Update 33: EYE AA moves both eyes' lenses by this frame's sub-pixel step
                    // (headset pixels); the history and the headset layer get the same lens.
                    std::array<std::array<float, 2>, 2> aaShift{}; // tangents, per eye
                    bool aaShifted = false;
                    if (!flatScreen && (eyeAaNow() & 0xff) == eye_taa::on &&
                        engine_taa::stateOf(SpidyEngineTaaNow()) != engine_taa::on) {
                        float jx{}, jy{};
                        eye_taa::step(eyes.serial, jx, jy);
                        for (unsigned i = 0; i < 2; ++i) {
                            auto& f = motion.fovs[i];
                            float dx{}, dy{};
                            if (eye_taa::lensShift(f.left, f.right, f.down, f.up, dimensions[0], dimensions[1], jx,
                                                   jy, dx, dy) &&
                                eye_taa::shiftLens(f.left, f.right, f.down, f.up, dx, dy)) {
                                aaShift[i] = {dx, dy};
                                aaShifted = true;
                            }
                        }
                    }
                    eyes.leaseMs = 250;
                    eyes.anchor = motion.anchor;
                    eyes.anchored = 1;
                    for (unsigned i = 0; i < 2; ++i) {
                        eyes.eyes[i].world = motion.eyes[i];
                        const auto& f = motion.fovs[i];
                        eyes.eyes[i].fov[0] = f.left;
                        eyes.eyes[i].fov[1] = f.right;
                        eyes.eyes[i].fov[2] = f.down;
                        eyes.eyes[i].fov[3] = f.up;
                    }
                    if (!flatScreen) {
                        // The views the eye history turns the last pair to this one with.
                        std::array<eye_history::EyeView, 2> views{};
                        for (unsigned i = 0; i < 2; ++i) {
                            views[i].world = motion.eyes[i];
                            const auto& f = motion.fovs[i];
                            views[i].tangents = {std::tan(f.left), std::tan(f.right), std::tan(f.down),
                                                 std::tan(f.up)};
                        }
                        native_gpu::noteEyeViews(eyes.serial, views);
                    }
                    // The HUD panel for this command's frame, as the head sees it:
                    // fixed in the room, so a reused image the headset turns to a
                    // newer head shows it where it was.
                    const Quat hudFacing = hudFollow.update(frame.head.orientation, frame.seconds);
                    native_hud::follow(eyes.serial, frame.head.orientation.conjugate() * hudFacing);
                    NativeEyeFrame saved;
                    saved.serial = eyes.serial;
                    saved.motion = motion;
                    saved.trackingEyes = frame.eyes;
                    if (aaShifted) // Update 33: the headset layer gets the lens the eyes were made with
                        for (unsigned i = 0; i < 2; ++i) {
                            auto& f = saved.trackingEyes[i].fov;
                            eye_taa::shiftLens(f.left, f.right, f.down, f.up, aaShift[i][0], aaShift[i][1]);
                        }
                    saved.flatScreen = flatScreen;
                    saved.bodyWanted = (values.body || strictBody) && !(strictBody && !strictVerified);
                    saved.uiStyle = values.uiStyle == 1 ? UiStyle::tech : UiStyle::comic;
                    saved.screenPose = screenPose;
                    saved.screenAspect = SpidyFlatAspect();
                    saved.slow = moments.view(slowMotion.view());
                    // Version 0.0.72 GAME BODY fallback (spec point 3): while the hero is not drawn as the
                    // body (the game's switch hides him), a glove on each tracked controller (glove_overlay.hpp).
                    // Test build 10: never over a shown suit; always for BATMAN without one (suit_wear::glovesShown).
                    if (suit_wear::glovesShown(strictBody, hero_modes::gameBodyShown(heroNow()), flatScreen, native_body::drawn(),
                                               suitShownNow()))
                        for (unsigned i = 0; i < 2; ++i) {
                            saved.gloveOn[i] = frame.hands[i].valid && validTrackedPose(frame.hands[i].grip) ? 1.f : 0.f;
                            saved.gloveFist[i] = gloveFist[i];
                        }
                    saved.health = heroShown.update(heroShare, frame.seconds);
                    // Update 38: this frame's world-from-tracking (RING ANCHOR HAND) and the
                    // web-ball globs in its world (BALL LOOK GLOB).
                    saved.worldFromTracking = compose(motion.headPose, inverse(frame.head));
                    latestToWorld = saved.worldFromTracking;
                    latestWrist = motion.hands[0];
                    latestWristTracked = motion.swing.hands[0].tracked;
                    for (unsigned i = 0; i < 2; ++i) {
                        latestGrips[i] = motion.grips[i];
                        latestGripsTracked[i] = motion.swing.hands[i].tracked && validTrackedPose(motion.grips[i]);
                    }
                    latestShown = frame.predictedDisplayTime;
                    for (unsigned g = 0; g < 2; ++g) { // Update 38.2 GRAB WEB's rope, this frame's world
                        saved.grabOn[g] = grabWebs[g].on && finite(grabFrom[g]) && finite(grabTo[g]) ? 1.f : 0.f;
                        if (saved.grabOn[g] > 0) {
                            saved.grabFrom[g] = compose(saved.worldFromTracking, Pose{grabFrom[g], {}}).position;
                            saved.grabTo[g] = compose(saved.worldFromTracking, Pose{grabTo[g], {}}).position;
                        }
                    }
                    {   // Version 0.0.3 WALL STICK's nets (world already) and SHOW STATUS's place.
                        const auto& nets = wallStick.netsNow();
                        for (size_t n = 0; n < saved.netAt.size() && n < nets.size(); ++n) {
                            saved.netFade[n] = finite(nets[n].at) && finite(nets[n].normal) ? nets[n].fade : 0.f;
                            saved.netAt[n] = nets[n].at;
                            saved.netNormal[n] = nets[n].normal;
                            saved.netSeed[n] = nets[n].seed;
                        }
                        saved.perfAtOk = validTrackedPose(motion.grips[0]) && motion.swing.hands[0].tracked ? 1.f : 0.f;
                        if (saved.perfAtOk > 0)
                            saved.perfAt = motion.grips[0].position + Vec3{0, .24f, 0};
                        // AUTO PERFORMANCE's zone: where the hero is (world x, z) and how fast he goes.
                        const float dt = std::isfinite(frame.seconds) ? std::clamp(frame.seconds, 0.f, .1f) : 0.f;
                        if (finite(motion.anchor)) {
                            if (perfHeroSet && dt > 1e-4f)
                                perfHeroSpeed.store(length(motion.anchor - perfHeroWas) / dt, std::memory_order_relaxed);
                            perfHeroWas = motion.anchor;
                            perfHeroSet = true;
                            perfHeroX.store(motion.anchor.x, std::memory_order_relaxed);
                            perfHeroZ.store(motion.anchor.z, std::memory_order_relaxed);
                        }
                        perfEnemyMetres.store(world_trace::nearestEnemyMetres(), std::memory_order_relaxed);
                    }
                    for (unsigned g = 0; g < 2; ++g) { // Update 38.3 GADGET HINT, 14 cm over the wrist
                        saved.hintFade[g] = gadgetHints[g].fade();
                        saved.hintLabel[g] = gadgetHints[g].label;
                        if (saved.hintFade[g] > 0 && validTrackedPose(motion.grips[g]))
                            saved.hintAt[g] = motion.grips[g].position + Vec3{0, .14f, 0}; // the grips are world
                        else
                            saved.hintFade[g] = 0;
                    }
                    for (int k = 0; k < web_cocoon::Set::most; ++k) { // Version 0.0.3 WEB BALL HIT, this frame's world
                        saved.cocoons[static_cast<size_t>(k)] = oursFault ? web_cocoon::Cocoon{} : cocoons.at(k);
                        auto& cc = saved.cocoons[static_cast<size_t>(k)];
                        if (cc.on)
                            cc.feet = compose(saved.worldFromTracking, Pose{cc.feet, {}}).position;
                    }
                    {   // Version 0.0.4 IRON MAN ABILITIES (REPULSOR): the blasts' look, the beam, the cooldown line.
                        const bool rep = !oursFault && c38(combat38::shooterType) == repulsor::repulsor;
                        saved.repulsorLook = rep;
                        saved.beamFade = rep ? repulsor::beamFade(static_cast<float>(combatClock - repBeamAt)) : 0.f;
                        if (saved.beamFade > 0) {
                            saved.beamFrom = compose(saved.worldFromTracking, Pose{repBeamFrom, {}}).position;
                            saved.beamTo = compose(saved.worldFromTracking, Pose{repBeamTo, {}}).position;
                            if (!finite(saved.beamFrom) || !finite(saved.beamTo))
                                saved.beamFade = 0;
                        }
                        // Version 0.0.5 BEAM RING: the cooldown on the right wrist (no floating line).
                        saved.beamRingOk = rep && validTrackedPose(motion.grips[1]) && motion.swing.hands[1].tracked ? 1.f : 0.f;
                        saved.beamShare = repulsor::chargeShare(repTrigger);
                        saved.beamFlash = repulsor::readyFlash(static_cast<float>(combatClock - repReadyAt));
                        // Version 0.0.62 HAND COOLDOWN: each hand's 2 s on its own wrist (shown while it cools).
                        for (unsigned i = 0; i < 2; ++i) {
                            saved.blastShare[i] = repulsor::handShare(repTrigger, static_cast<int>(i));
                            saved.blastFlash[i] = repulsor::handReadyFlash(repTrigger, static_cast<int>(i));
                            saved.blastRingOk[i] = rep && validTrackedPose(motion.grips[i]) && motion.swing.hands[i].tracked &&
                                                           (saved.blastShare[i] < 1.f || saved.blastFlash[i] > 0)
                                                       ? 1.f
                                                       : 0.f;
                        }
                        // Version 0.0.6 (Update 39.0.2) REPULSOR LOOK: each palm's charge (the web shooter's
                        // place, facing along the hand's aim), this frame's world; glow 0: none.
                        for (unsigned i = 0; i < 2; ++i) {
                            saved.palmGlow[i] = 0;
                            if (!rep || !validTrackedPose(motion.grips[i]) || !motion.swing.hands[i].tracked)
                                continue;
                            const float glow = repulsor_mcu::palmGlow(repTrigger.chargingFor(static_cast<int>(i)),
                                                                      repTrigger.sinceBlast(static_cast<int>(i)));
                            // Version 0.0.62 PALM BLAST: the charge and its flash on the palm, facing out of it.
                            const Vec3 at = compose(saved.worldFromTracking,
                                                    Pose{repulsor::palmCentre(motion.grips[i], i), {0, 0, 0, 1}}).position;
                            const Vec3 way = saved.worldFromTracking.orientation.rotate(repulsor::palmNormal(motion.grips[i], i));
                            if (glow > 0 && finite(at) && finite(way) && length(way) > .5f) {
                                saved.palmGlow[i] = glow;
                                saved.palmAt[i] = at;
                                saved.palmWay[i] = normalized(way);
                            }
                        }
                        saved.palmSpin = static_cast<float>(std::fmod(combatClock * 9.0, 2.0 * 3.14159265));
                    }
                    {   // Version 0.0.6 DAREDEVIL ABILITIES: the batons, in the hand (0.0.7: in the fist) or spinning in flight.
                        const bool dd = !oursFault && heroWas == hero_modes::daredevil;
                        for (unsigned i = 0; i < 2; ++i) {
                            const auto& b = batons[i];
                            saved.batonOn[i] = 0;
                            saved.batonHeld[i] = false;
                            if (!dd || (b.inHandNow() && (!batonTracked[i] || !(length(batonAimNow[i]) > .5f))))
                                continue;
                            // Version 0.0.7 BATON HOLSTERS: a holstered baton is drawn on the back (hero007).
                            if (!hero007Play.batonShown(i, b.inHandNow(), hero007In.batonHolsters))
                                continue;
                            Vec3 grip{}, tip{};
                            if (b.inHandNow() && batonShaftOk[i]) {
                                // Version 0.0.7 BATON GRIP: in the fist, from the grip pose (baton_grip.hpp).
                                grip = batonButt[i];
                                tip = batonTipHeld[i];
                            } else if (b.inHandNow()) { // no usable grip pose: 0.0.62's, along the aim
                                const Vec3 aimDir = normalized(batonAimNow[i]);
                                grip = batonHand[i] - aimDir * .12f;
                                tip = batonHand[i] + aimDir * .45f;
                            } else {
                                const Vec3 way = length(b.velocity) > .1f ? normalized(b.velocity) : Vec3{0, 0, -1};
                                const Vec3 flat = cross(way, Vec3{0, 1, 0});
                                const Vec3 across = length(flat) > .1f ? normalized(flat) : Vec3{1, 0, 0};
                                const float spin = b.out * 18.f;
                                const Vec3 axis = way * std::cos(spin) + across * std::sin(spin);
                                grip = b.position - axis * .28f;
                                tip = b.position + axis * .28f;
                            }
                            grip = compose(saved.worldFromTracking, Pose{grip, {}}).position;
                            tip = compose(saved.worldFromTracking, Pose{tip, {}}).position;
                            if (finite(grip) && finite(tip)) {
                                saved.batonGrip[i] = grip;
                                saved.batonTip[i] = tip;
                                saved.batonOn[i] = 1;
                                saved.batonHeld[i] = b.inHandNow() && batonShaftOk[i]; // the hand at the shaft's third
                            }
                        }
                        // Version 0.0.7 BATON LOOK (MODEL: the converted batons, CLUB when they are missing).
                        const uint32_t batonLookNow = c38(combat38::batonLook);
                        saved.batonClub = combat38::batonClub(batonLookNow) || combat38::batonModel(batonLookNow);
                        // Version 0.0.7 HERO PROFILES: the heroes' weapons, holsters, beams, smoke and shells.
                        if (!oursFault)
                        {
                            const bool pulling = capeNow.grappling == cape_glide::pull && capeNow.hand < 2;
                            for (unsigned h = 0; h < 2; ++h) // Version 0.0.72: the grapple gun out while it fires / pulls
                                hero007In.grappleHeld[h] = grapple072::deviceOf(hero007In.hero) == grapple072::Device::gun &&
                                                           ((pulling && capeNow.hand == h) ||
                                                            (hero007In.hero == hero_modes::punisher && hero072HookOut[h]));
                            hero007Play.draw(saved.hero007, saved.worldFromTracking, hero007In, batonLookNow);
                            saved.hero072 = hero072::worldLook(hero072Look, saved.worldFromTracking); // Version 0.0.72
                            // Version 0.0.7 BATMAN GRAPPLE GUN: the line from the firing hand to the hook (game
                            // world, the bridge's) while it pulls him; never a web.
                            // Version 0.0.72: every grappler's - from the gun's muzzle (BATMAN, the PUNISHER:
                            // hero007_visual moves it there) or DAREDEVIL's baton tip.
                            const uint32_t gh = hero007In.hero;
                            const bool dd = gh == hero_modes::daredevil;
                            if (grapple072::grappler(gh) && pulling &&
                                (dd ? hero072HandTracked[capeNow.hand] : hero007In.hands[capeNow.hand].tracked)) {
                                const unsigned h = capeNow.hand;
                                const Vec3 at = dd ? grapple072::cableFrom(hero072HandAt[h], hero072HandAim[h]) : hero007In.hands[h].position;
                                const Vec3 from = compose(saved.worldFromTracking, Pose{at, {}}).position;
                                if (finite(from) && finite(capeNow.anchor)) {
                                    saved.hero007.grapple[h] = true;
                                    saved.hero007.grappleFrom[h] = from;
                                    saved.hero007.grappleTo[h] = capeNow.anchor;
                                    // Version 0.0.72: the line shoots out of the muzzle as the pull starts.
                                    if (!grappleLineWas[h])
                                        grappleLineAt[h] = std::chrono::steady_clock::now();
                                    saved.hero007.grappleOut[h] = hero007::grappleOut(std::chrono::duration<float>(
                                        std::chrono::steady_clock::now() - grappleLineAt[h]).count());
                                    saved.hero007.any = true;
                                }
                            }
                            for (unsigned h = 0; h < 2; ++h)
                                grappleLineWas[h] = saved.hero007.grapple[h];
                            // Version 0.0.7 BATARANG STYLE / GUN MODELS / BATON LOOK MODEL: the converted models
                            // under <Spidy folder>\Models\ (each slot read once, on first use; the built-in look
                            // when missing or refused - never a crash, nothing else stops).
                            auto& models = mesh_model::library();
                            const auto slotModel = [&](uint32_t slot, const char* what) -> const mesh_model::Mesh* {
                                if (slot == 0 || slot >= mesh_model::slots)
                                    return nullptr; // build 5: MODEL MISSING never reads out of range
                                if (models.status[slot] == mesh_model::notTried) {
                                    mesh_model::Status status = mesh_model::invalid;
                                    try {
                                        status = models.load(slot, modelsRoot());
                                    } catch (...) { // out of memory: MODEL MISSING, never a crash
                                        models.meshes[slot] = {};
                                        models.status[slot] = mesh_model::invalid;
                                        models.error[slot] = "could not be read (out of memory?)";
                                    }
                                    char line[400];
                                    sprintf_s(line, "Version 0.0.7 %s %s: %s%s%.300s", what, mesh_model::slotFolders[slot],
                                              mesh_model::statusNames[status < 5 ? static_cast<int>(status) : 0],
                                              status == mesh_model::loaded ? "" : " - the built-in look drawn: ", models.error[slot].c_str());
                                    game_prompts::note(line);
                                    if (status != mesh_model::loaded && slot < 32)
                                        modelMissingMask.fetch_or(1u << slot);
                                }
                                if (models.status[slot] != mesh_model::loaded && slot < 32) // (read at start: 0.0.72)
                                    modelMissingMask.fetch_or(1u << slot);
                                return models.model(slot);
                            };
                            const uint32_t style = c38(combat38::batarangStyle);
                            // Version 0.0.72 MODEL MISSING: a look whose MODEL setting is on is the model or nothing.
                            if (style && style < mesh_model::styles && hero007In.hero == hero_modes::batman) {
                                saved.hero007.batarangModel = slotModel(style, "BATARANG STYLE");
                                saved.hero007.modelWanted |= hero007::modelBitBatarang;
                            }
                            // BATMAN GRAPPLE GUN model (Models\GrappleGun): none drawn when it is missing.
                            if (hero007In.hero == hero_modes::batman || hero007In.hero == hero_modes::punisher) // 0.0.72: his hook too
                                saved.hero007.grappleGunModel = slotModel(mesh_model::slotGrappleGun, "GRAPPLE GUN");
                            // Version 0.0.72 THOR's hammer (Models\Hammer\<THOR HAMMER's pick>): the model or nothing,
                            // MODEL MISSING on the wrist.
                            if (saved.hero072.hammer && c38(combat38::thorHammerModel) != 1) { // (only Thor holds it; build 6: THOR HAMMER MODEL OFF draws none)
                                if (thorStepsLogged < 2) {
                                    thorStepsLogged = 2;
                                    consoleLine("THOR: step 2 hammer model (THOR HAMMER " + std::to_string(c38(combat38::thorHammer)) + ")");
                                }
                                saved.hero072.hammerModel =
                                    slotModel(mesh_model::hammerSlot(c38(combat38::thorHammer)), "THOR HAMMER");
                                if (thorStepsLogged < 3) {
                                    thorStepsLogged = 3;
                                    consoleLine(std::string("THOR: step 3 hammer model ") + (saved.hero072.hammerModel ? "drawn" : "MISSING (none drawn)"));
                                }
                            }
                            // Version 0.0.72 PUNISHER VAN (EXPERIMENTAL): the Vandura, its built cab and its wheel - each the
                            // model or nothing (MODEL MISSING on the wrist); drawn where the van is (world space).
                            if (c38(combat38::punisherVan) == 1 && hero007In.hero == hero_modes::punisher) {
                                const mesh_model::Mesh* body = slotModel(mesh_model::slotVan, "PUNISHER VAN");
                                const mesh_model::Mesh* cab = slotModel(mesh_model::slotVanInterior, "PUNISHER VAN");
                                const mesh_model::Mesh* wheel = slotModel(mesh_model::slotVanWheel, "PUNISHER VAN");
                                if (body && van.state != van072::none && finite(van.frame.at) && std::isfinite(van.frame.yaw)) {
                                    auto& L = saved.hero072;
                                    L.van = L.any = true;
                                    L.vanModel = body;
                                    L.vanInterior = cab;
                                    L.vanWheel = wheel;
                                    L.vanAt = van.frame.at;
                                    L.vanForward = van.frame.forward();
                                    Vec3 up{}, toward{};
                                    van072::wheelAxes(van.wheel.angle, up, toward);
                                    L.wheelAt = van.frame.world(van072::wheelHub);
                                    L.wheelForward = -van072::worldDir(van.frame, toward);
                                    L.wheelUp = van072::worldDir(van.frame, up);
                                }
                            }
                            if (combat38::gunModelsOn(c38(combat38::gunModels)) && hero007In.hero == hero_modes::punisher) {
                                saved.hero007.pistolModel = slotModel(mesh_model::slotPistol, "GUN MODELS");
                                saved.hero007.sawedFrame = slotModel(mesh_model::slotSawedOffFrame, "GUN MODELS");
                                saved.hero007.sawedBarrels = slotModel(mesh_model::slotSawedOffBarrels, "GUN MODELS");
                                saved.hero007.modelWanted |= hero007::modelBitGuns;
                            }
                            // Version 0.0.7 CAPTAIN AMERICA SHIELD LOOK MODEL (Models\Shield; BUILT-IN when missing).
                            if (captain::lookModel(c38(combat38::shieldLook)) && hero007In.hero == hero_modes::captainAmerica)
                            {
                                saved.hero007.shieldModel = slotModel(mesh_model::slotShield, "SHIELD LOOK");
                                saved.hero007.modelWanted |= hero007::modelBitShield;
                            }
                            if (combat38::batonModel(batonLookNow) && dd) {
                                saved.hero007.modelWanted |= hero007::modelBitBaton;
                                for (unsigned i = 0; i < 2; ++i) {
                                    saved.hero007.batonModel[i] = slotModel(i ? mesh_model::slotBatonRight : mesh_model::slotBatonLeft, "BATON LOOK");
                                    // The chain: ONLY while this baton is out of the hand (thrown, bouncing, coming back).
                                    const auto& b = batons[i];
                                    if (saved.hero007.batonModel[i] && saved.batonOn[i] > 0 && !b.inHandNow() && batonTracked[i]) {
                                        const Vec3 from = compose(saved.worldFromTracking, Pose{batonHand[i], {}}).position;
                                        const Vec3 to = compose(saved.worldFromTracking, Pose{b.position, {}}).position;
                                        if (finite(from) && finite(to)) {
                                            saved.hero007.cable[i] = true;
                                            saved.hero007.cableFrom[i] = from;
                                            saved.hero007.cableTo[i] = to;
                                            saved.hero007.any = true;
                                        }
                                    }
                                }
                            }
                        }
                        else {
                            saved.hero007 = hero007::Draw{};
                            saved.hero072 = hero072::Look{};
                        }
                    }
                    {   // Version 0.0.5 SUPERSONIC: the shockwave around the head, the way the hands point.
                        saved.shockFade = fly_boost::shock(static_cast<float>(combatClock - flyBoomAt));
                        Vec3 way{};
                        for (unsigned i = 0; i < 2; ++i)
                            if (motion.swing.hands[i].tracked)
                                way = way + motion.swing.hands[i].aim.orientation.rotate({0, 0, -1});
                        if (!(finite(way) && length(way) > .3f))
                            way = motion.headPose.orientation.rotate({0, 0, -1});
                        saved.shockWay = saved.worldFromTracking.orientation.rotate(way);
                        if (!finite(saved.shockWay) || length(saved.shockWay) < .3f)
                            saved.shockFade = 0;
                        else
                            saved.shockWay = normalized(saved.shockWay);
                        saved.shockAge = static_cast<float>(combatClock - flyBoomAt);
                    }
                    if (!oursFault) // Version 0.0.2 BALL LOOK OURS: our balls and splashes, this frame's world
                        for (size_t b = 0; b < saved.oursAt.size() && b < static_cast<size_t>(our_ball::Pool::balls); ++b) {
                            const auto& ball = oursBalls.ball(static_cast<int>(b));
                            if (ball.alive) {
                                saved.oursAt[b] = compose(saved.worldFromTracking, Pose{ball.at, {}}).position;
                                saved.oursTail[b] = compose(saved.worldFromTracking, Pose{ball.tail(), {}}).position;
                                saved.oursFade[b] = 1;
                            }
                            if (oursSplash[b].fade() > 0) {
                                saved.splashAt[b] = compose(saved.worldFromTracking, Pose{oursSplash[b].at, {}}).position;
                                saved.splashFade[b] = oursSplash[b].fade();
                                if (saved.repulsorLook) // Version 0.0.6 (Update 39.0.2): the 0.18 s impact burst
                                    saved.splashFade[b] = repulsor_mcu::impactLife(oursSplash[b].age);
                            }
                        }
                    // Version 0.0.62 HIT INDICATOR (ON only): the marks, this frame's world.
                    static_assert(hit_marks::most == 8, "NativeEyeFrame markAt/markFade hold 8");
                    if (c38(combat38::hitIndicator) == 1)
                        for (int m = 0; m < hit_marks::most; ++m)
                            if (const float f = hitMarks.fade(m); f > 0) {
                                saved.markAt[static_cast<size_t>(m)] = compose(saved.worldFromTracking, Pose{hitMarks.at(m), {}}).position;
                                saved.markFade[static_cast<size_t>(m)] = finite(saved.markAt[static_cast<size_t>(m)]) ? f : 0.f;
                            }
                    if (trainDraw.on) { // Version 0.0.2 TRAINING's guide, this frame's world
                        auto& gd = saved.guide;
                        gd = trainDraw;
                        const auto toWorld = [&](Vec3 p) { return compose(saved.worldFromTracking, Pose{p, {}}).position; };
                        for (int h = 0; h < 2; ++h) {
                            for (int k = 0; k < training_guide::points; ++k)
                                gd.p[h][k] = toWorld(trainDraw.p[h][k]);
                            gd.ghost[h] = toWorld(trainDraw.ghost[h]);
                        }
                        saved.guideAt = toWorld(trainGuide.labelAt());
                        memcpy(saved.guideLine, trainLine, sizeof trainLine);
                    }
                    for (unsigned g = 0; g < 2; ++g)
                        if (globs[g].alive) {
                            saved.globAt[g] = compose(saved.worldFromTracking, Pose{globs[g].at(), {}}).position;
                            saved.globTail[g] = compose(saved.worldFromTracking, Pose{globs[g].tail(), {}}).position;
                            saved.globFade[g] = std::clamp(1.f - globs[g].age / combat38::Glob::life, 0.f, 1.f);
                        }
                    if (gadgetWheel.open()) {
                        // The gadget wheel, from tracking space into this frame's
                        // world, with the gadgets in their places.
                        const Pose toWorld = compose(motion.headPose, inverse(frame.head));
                        const auto& f = gadgetWheel.frame();
                        auto& view = saved.gadgetWheel;
                        view.open = true;
                        view.age = gadgetWheel.age();
                        view.litAge = gadgetWheel.litAge();
                        view.lit = gadgetWheel.lit();
                        view.frame = {compose(toWorld, Pose{f.centre, {}}).position, toWorld.orientation.rotate(f.right),
                                      toWorld.orientation.rotate(f.up), toWorld.orientation.rotate(f.out)};
                        wheelPlaces(view);
                    }
                    if (phone.live()) {
                        // The phone: this image's way from tracking space into its
                        // world, and the screen as it is now, which every image
                        // shows (phoneShown).
                        saved.phone = {true, compose(motion.headPose, inverse(frame.head))};
                        auto& view = phoneShown;
                        view.screen = phone.ui().screen();
                        view.app = phone.ui().app();
                        view.lit = phone.ui().button();
                        // GEAR: the gadget wheel's places in its order, the web
                        // shooter's first, and the one equipped as the wheel has it.
                        GadgetWheelView wheel;
                        wheelPlaces(wheel);
                        view.gearCount = 0;
                        for (int p = 0; p < gadgetPlaces; ++p) {
                            if (wheel.places[p] == Gadget::none)
                                continue;
                            auto& row = view.gear[view.gearCount++];
                            strncpy_s(row.name, gadgetName(wheel.places[p]), _TRUNCATE);
                            row.ammo = wheel.ammo[p];
                            row.most = wheel.most[p];
                            row.equipped = wheel.equipped == p;
                        }
                        view.gearTop = phone.ui().gearTop();
                        const auto music = phoneMusic.status();
                        view.playing = music.playing;
                        view.autoplay = music.autoplay;
                        view.failed = music.failed;
                        view.track = music.track;
                        view.tracks = music.tracks;
                        view.volume = music.volume;
                        strncpy_s(view.song, music.song, _TRUNCATE);
                    }
                    for (unsigned i = 0; i < 2; ++i) {
                        saved.webs[i] = swingState.webs[i];
                        saved.webTimes[i] = webTimes[i];
                        saved.aims[i] = aims.hands[i];
                        saved.offers[i] = pointing[i] ? *pointing[i] : offer_pointing::Pointed{};
                    }
                    saved.webSlots = {webRopes.slot(0), webRopes.slot(1)};
                    saved.strandUpper = webRopes.upper();
                    if (calibration.phase() != body_calibration::Phase::idle) {
                        // The panel stays where it appeared in the room; the tracking
                        // space is where this frame's head puts it in the world.
                        auto& view = saved.calibration;
                        view.phase = calibration.phase();
                        view.hint = calibration.hint();
                        view.progress = calibration.progress();
                        view.ready = calibration.armsReady();
                        view.result = calibration.result();
                        view.result.eyeHeight = calibratedEye;
                        view.result.armLength = calibratedArm;
                        view.panel = compose(compose(motion.headPose, inverse(frame.head)), calibrationPanel);
                        for (unsigned i = 0; i < 2; ++i) {
                            view.handTracked[i] = frame.hands[i].valid && validTrackedPose(frame.hands[i].grip);
                            view.grips[i] = motion.grips[i];
                        }
                    }
                    saved.notice = testNoticeFault ? test_notice::View{}
                                                   : test_notice::view(testNotice, compose(motion.headPose, inverse(frame.head)));
                    saved.roster = rosterFault || (!rosterPage.isOpen() && rosterPage.opacity() <= 0)
                                       ? roster_page::View{}
                                       : roster_page::view(rosterPage, static_cast<uint32_t>(wrist_hud::activeHero()),
                                                           compose(motion.headPose, inverse(frame.head)));
                    history.remember(saved);
                    const auto code = SpidySetEyes(&eyes);
                    if (code && code != 4003)
                        check(code, "Tracked eye command");
                    return !code;
                },
                [&](std::array<XrRuntime::EyeTarget, 2>& targets) {
                    const auto copyStart = std::chrono::steady_clock::now();
                    if (showScreen && stereoScene && !native_gpu::eyesLost() && native_hud::onScreen(sceneHoldMs)) {
                        // The scene's newest pair, once the game has rendered
                        // one with the HUD's panel on its screen; until then
                        // the game's own frame below.
                        uint64_t pairSerial{}, pairGeneration{};
                        const bool copied = native_gpu::copyLatest(targets[0].texture, targets[1].texture,
                                                                   pairSerial, pairGeneration);
                        check(native_gpu::error(), "GPU eye bridge");
                        const auto* saved = copied ? history.find(pairSerial, displayTime) : nullptr;
                        if (saved && saved->stereoScreen) {
                            copyMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                               copyStart)
                                         .count();
                            runtime.presentation(true, screenPose, saved->screenAspect,
                                                 vr_settings::screenWidth(values.screenSize), true);
                            drawn = Presentation::stereoScreen;
                            D3D12Renderer::Captured pixels;
                            if (snapshotTicket && overlay.captured(snapshotTicket, pixels)) {
                                publishSnapshot(pendingSnapshot, &pixels);
                                snapshotTicket = 0;
                            }
                            if (!snapshotTicket && snapshots.due(GetTickCount64(), 0, false) &&
                                (snapshotTicket = overlay.capture(targets[0].texture)) != 0) {
                                pendingSnapshot = {};
                                pendingSnapshot.flatScreen = 1;
                                pendingSnapshot.generation = pairGeneration;
                                pendingSnapshot.serial = pairSerial;
                            }
                            std::lock_guard lock(telemetry);
                            InterlockedIncrement64(&SpidyXrData.sequence);
                            SpidyXrData.serial = pairSerial;
                            SpidyXrData.generation = pairGeneration;
                            InterlockedIncrement64(&SpidyXrData.sequence);
                            return true;
                        }
                    }
                    native_gpu::Screen image;
                    if (showScreen && native_gpu::latestScreen(image, imageHoldMs)) {
                        // The game's presented frame, scaled into the image the screen shows.
                        const auto& left = targets[0];
                        overlay.blit(image.texture.Get(), image.view, image.linear,
                                     {left.texture, left.format, left.width, left.height, {}});
                        // Update 32.1: why the headset shows the game screen.
                        if (native_gpu::eyesLost() && !overlayFault) {
                            try {
                                const float aspect = static_cast<float>(left.width) / static_cast<float>(left.height);
                                std::vector<Vertex> notice, backing;
                                eye_swap::appendLostNotice(notice, aspect);
                                eye_swap::appendLostBacking(backing, aspect);
                                const std::array<D3D12Renderer::ViewTarget, 1> noticeView{
                                    {{left.texture, left.format, left.width, left.height,
                                      eye_swap::noticeProjection(aspect)}}};
                                overlay.renderViews(noticeView, notice, true, false, backing);
                            } catch (const std::exception& e) {
                                overlayFault = true;
                                const auto text = std::string("VR continues; Spidy's drawings are off this session: ") +
                                                  e.what();
                                message(3, 0, text.c_str());
                            }
                        }
                        copyMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                           copyStart)
                                     .count();
                        runtime.presentation(true, screenPose,
                                             static_cast<float>(image.width) / static_cast<float>(image.height),
                                             vr_settings::screenWidth(values.screenSize));
                        drawn = Presentation::gameScreen;
                        D3D12Renderer::Captured pixels;
                        if (snapshotTicket && overlay.captured(snapshotTicket, pixels)) {
                            publishSnapshot(pendingSnapshot, &pixels);
                            snapshotTicket = 0;
                        }
                        if (!snapshotTicket && snapshots.due(GetTickCount64(), 0, false) &&
                            (snapshotTicket = overlay.capture(left.texture)) != 0) {
                            pendingSnapshot = {};
                            pendingSnapshot.flatScreen = 1;
                        }
                        std::lock_guard lock(telemetry);
                        InterlockedIncrement64(&SpidyXrData.sequence);
                        SpidyXrData.serial = SpidyXrData.generation = 0;
                        InterlockedIncrement64(&SpidyXrData.sequence);
                        return true;
                    }
                    uint64_t imageSerial{}, imageGeneration{};
                    bool copied = native_gpu::copyLatest(targets[0].texture, targets[1].texture, imageSerial,
                                                         imageGeneration);
                    // Update 16: the FPS page's count: a headset frame showing
                    // an eye pair it has not shown before is a new frame.
                    {
                        const bool newPair = copied && (imageSerial != wristFpsSerial ||
                                                         imageGeneration != wristFpsGeneration);
                        if (newPair) {
                            wristFpsSerial = imageSerial;
                            wristFpsGeneration = imageGeneration;
                        }
                        const double frameSeconds = std::chrono::duration<double>(copyStart.time_since_epoch()).count();
                        wristFps.frame(frameSeconds, newPair, runtime.lastFrameTiming().period);
                        if (newPair) { // Version 0.0.3 AUTO PERFORMANCE: each new eye pair's time
                            perfNewPair = true;
                            perfPairSeconds = frameSeconds;
                        }
                        // Update 24: the session's, the same new pairs.
                        wristSession.frame(frameSeconds, newPair);
                        {   // Update 37.2: COLOR and FOVEATED taken by the eye copy (sent again until it is
                            // running), and FOVEATED's per-level FPS / eye GPU ms once a second (SpidyFoveaData).
                            static double lastSent{}, perfSecond{};
                            static uint32_t pairs{}, faultLogged{};
                            if (frameSeconds - lastSent >= .25 || frameSeconds < lastSent) {
                                lastSent = frameSeconds;
                                // Version 0.0.6 DAREDEVIL SONAR: on and off with DAREDEVIL ABILITIES.
                                static bool sonarWas{};
                                if (const bool sonarNow = heroNow() == hero_modes::daredevil; sonarNow != sonarWas) {
                                    sonarWas = sonarNow;
                                    colorDirty = true;
                                }
                                if (colorDirty.exchange(false)) {
                                    const auto want = colorWanted();
                                    const bool ok = native_gpu::setEyeColor(want);
                                    colorRefused = !ok && want.on;
                                    if (!ok && want.on && native_gpu::eyeSharpenFault() == 0)
                                        colorDirty = true; // the pass is not up yet: again
                                }
                                // Update 38.5: 90s ANIMATED's shader failed on this GPU: told once, the FILTER
                                // back to NONE (the grade and the sharpening carry on; never the flat screen).
                                if (const uint32_t tf = native_gpu::eyeToonFault();
                                    tf && colorFilter.load() == eye_color::animated90s) {
                                    colorFilter = eye_color::none;
                                    colorDirty = true;
                                    if (!toonNoted.exchange(true))
                                        game_prompts::note(tf == 1 ? "90s ANIMATED could not start (its shader failed): FILTER set to NONE"
                                                               : "90s ANIMATED could not start (its pipeline failed): FILTER set to NONE");
                                }
                                if (static bool sonarNoted{}; !sonarNoted && sonarWas &&
                                                                  (native_gpu::eyeToonFault() || native_gpu::eyeSharpenFault())) {
                                    sonarNoted = true;
                                    game_prompts::note("VERSION 0.0.6 DAREDEVIL SONAR could not start (the eye pass failed): "
                                                       "the game's look stays");
                                }
                                if (foveaDirty.exchange(false)) {
                                    const uint32_t level = foveaLevelWant.load();
                                    const bool ok = native_gpu::setFoveation(level, foveaInnerWant.load(), foveaDebugWant.load());
                                    const auto fs = native_gpu::foveaStatus();
                                    if (!ok && level != foveation::off && fs.fault == foveation::fine)
                                        foveaDirty = true; // the eye copy not running yet: again
                                }
                            }
                            if (foveaResetAsked.exchange(false))
                                foveaPerf.reset();
                            pairs += newPair ? 1u : 0u;
                            if (frameSeconds < perfSecond)
                                perfSecond = frameSeconds;
                            if (frameSeconds - perfSecond >= 1) {
                                const auto fs = native_gpu::foveaStatus();
                                const uint32_t level = fs.on ? fs.level : foveation::off;
                                const float fps = static_cast<float>(pairs / (frameSeconds - perfSecond));
                                if (perfSecond > 0)
                                    foveaPerf.note(level, fps, static_cast<float>(native_gpu::eyePassesGpuMs()));
                                perfSecond = frameSeconds;
                                pairs = 0;
                                if (fs.fault && faultLogged != fs.fault) {
                                    faultLogged = fs.fault;
                                    char line[160];
                                    std::snprintf(line, sizeof line, "Update 37.2: FOVEATED turned itself off: %s",
                                                  foveation::faultName(fs.fault));
                                    game_prompts::note(line);
                                }
                                publish(SpidyFoveaData, [&](foveation::Data& d) {
                                    d.level = fs.level;
                                    d.inner = fs.inner;
                                    d.debug = fs.debug ? 1u : 0u;
                                    d.tier = fs.tier;
                                    d.tile = fs.tile;
                                    d.has4x4 = fs.has4x4 ? 1u : 0u;
                                    d.fault = fs.fault;
                                    d.shadedShare = fs.shadedShare;
                                    d.centreU = fs.centres[0];
                                    d.centreV = fs.centres[1];
                                    d.taggedPasses = fs.taggedPasses;
                                    d.appliedPasses = fs.appliedPasses;
                                    d.untaggedPasses = fs.untaggedPasses;
                                    d.resets = fs.resets;
                                    d.images = fs.images;
                                    d.mismatches = fs.mismatches;
                                    for (uint32_t l = 0; l < foveation::levels; ++l) {
                                        const auto& st = foveaPerf.at(l);
                                        d.perLevel[l].seconds = static_cast<uint32_t>(st.frames);
                                        d.perLevel[l].avgFps = st.avg();
                                        d.perLevel[l].lowFps = st.frames ? st.low : 0.f;
                                        d.perLevel[l].highFps = st.high;
                                        const float g = st.gpuAvg();
                                        d.perLevel[l].gpuMs = std::isfinite(g) ? g : 0.f;
                                    }
                                });
                                const auto cc = colorWanted();
                                publish(SpidyColorData, [&](eye_color::Data& d) {
                                    d.on = native_gpu::eyeColorActive() ? 1u : 0u;
                                    d.filter = cc.filter;
                                    d.strength = cc.strength;
                                    d.contrast = cc.contrast;
                                    d.saturation = cc.saturation;
                                    d.exposureTenths = cc.exposureTenths;
                                    d.warmth = cc.warmth;
                                    d.tint = cc.tint;
                                    d.vibrance = cc.vibrance;
                                    d.vignette = cc.vignette;
                                    d.fault = colorRefused.load() ? 1u : 0u;
                                    d.cartoon = cc.cartoon;
                                    d.toonBits = (cc.outlines ? 1u : 0u) | (cc.bands & 0xff) << 8 |
                                                 (native_gpu::eyeToonFault() & 0xff) << 16;
                                    d.gradedPairs = native_gpu::eyeColorPairs();
                                });
                            }
                        }
                        {   // Update 34: ENGINE TAA's FPS guard and its plan B (EYE AA).
                            static engine_taa::FpsGuard guard;
                            static uint32_t previous{};
                            if (engineTaaArm.exchange(false))
                                guard.arm(frameSeconds);
                            guard.frame(frameSeconds, newPair);
                            uint32_t engine = SpidyEngineTaaNow();
                            if (engine_taa::stateOf(engine) == engine_taa::on) {
                                if (guard.collapsed()) {
                                    char line[128];
                                    std::snprintf(line, sizeof line,
                                                  "ENGINE TAA: %.1f eye pairs/s before, %.1f after: off\n",
                                                  guard.before(), guard.after());
                                    OutputDebugStringA(line);
                                    SpidyEngineTaa(reinterpret_cast<void*>(uintptr_t{2}));
                                    engine = SpidyEngineTaaNow();
                                    guard.disarm(frameSeconds);
                                } else if (guard.decided()) {
                                    guard.disarm(frameSeconds);
                                }
                            } else if (guard.armed()) {
                                guard.disarm(frameSeconds);
                            }
                            if (engine_taa::stateOf(previous) == engine_taa::on &&
                                engine_taa::stateOf(engine) != engine_taa::on)
                                engineTaaFallback(engine_taa::reasonOf(engine));
                            previous = engine;
                        }
                        {   // Update 35: FAR SHADOWS' FPS guard: a mode the press chose that halves
                            // the frame rate goes back to OFF (the reason shown).
                            static engine_taa::FpsGuard guard;
                            if (farShadowsArm.exchange(false))
                                guard.arm(frameSeconds);
                            guard.frame(frameSeconds, newPair);
                            if (far_shadows::modeOf(SpidyFarShadowsNow()) == far_shadows::off) {
                                if (guard.armed())
                                    guard.disarm(frameSeconds);
                            } else if (guard.collapsed()) {
                                char line[128];
                                std::snprintf(line, sizeof line,
                                              "FAR SHADOWS: %.1f eye pairs/s before, %.1f after: off\n",
                                              guard.before(), guard.after());
                                OutputDebugStringA(line);
                                SpidyFarShadows(reinterpret_cast<void*>(uintptr_t{far_shadows::slowOff}));
                                guard.disarm(frameSeconds);
                            } else if (guard.decided()) {
                                guard.disarm(frameSeconds);
                            }
                        }
                        sessionFpsAvg = wrist_hud::fpsTenths(wristSession.average());
                        sessionFpsHigh = wrist_hud::fpsTenths(wristSession.high());
                        sessionFpsLow = wrist_hud::fpsTenths(wristSession.low());
                        sessionFpsSeconds = static_cast<uint32_t>(std::min(wristSession.trackedSeconds(), 262143.));
                    }
                    check(native_gpu::error(), "GPU eye bridge");
                    const auto* saved = copied ? history.find(imageSerial, displayTime) : nullptr;
                    copied = copied && saved;
                    copyMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                       copyStart)
                                 .count();
                    if (copied) {
                        // A scene's pair stays on its screen until play's first image.
                        drawn = saved->stereoScreen ? Presentation::stereoScreen
                                : saved->flatScreen ? Presentation::flat
                                                    : Presentation::immersive;
                        runtime.presentation(saved->flatScreen, saved->screenPose, saved->screenAspect,
                                             vr_settings::screenWidth(values.screenSize), saved->stereoScreen);
                        const auto& rendered = saved->motion;
                        // Update 26: with EYE TEST SAME PICTURE the right image IS
                        // the left eye's picture, made with the left eye's lens
                        // (mirror-image tangents) and pose. Shown through the right
                        // eye's own lens it slid and stretched sideways, so the
                        // right layer (and Spidy's drawings on it) now get the
                        // view that picture was made from: flat, but not warped.
                        const bool samePicture = eye_debug::eyeDebugSource(native_gpu::pairEyeDebug(), 1) == 0;
                        const auto made = [samePicture](unsigned eye) { return samePicture ? 0u : eye; };
                        for (unsigned eye = 0; eye < 2; ++eye) {
                            const auto& pose = saved->trackingEyes[made(eye)].pose;
                            const auto& f = saved->trackingEyes[made(eye)].fov;
                            targets[eye].view.pose = {{pose.orientation.x, pose.orientation.y,
                                                       pose.orientation.z, pose.orientation.w},
                                                      {pose.position.x, pose.position.y, pose.position.z}};
                            targets[eye].view.fov = {f.left, f.right, f.up, f.down};
                        }
                        const auto overlayStart = std::chrono::steady_clock::now();
                        // The native views moved this command's eyes with the
                        // player to the rendered frame. Hands move with them, so
                        // world-space web anchors line up with the rendered city.
                        Vec3 travel{};
                        native_eyes::renderOffset(imageGeneration, travel);
                        const Vec3 viewer =
                            Vec3{(rendered.eyes[0][12] + rendered.eyes[1][12]) / 2,
                                 (rendered.eyes[0][13] + rendered.eyes[1][13]) / 2,
                                 (rendered.eyes[0][14] + rendered.eyes[1][14]) / 2} +
                            travel;
                        const auto& lens = rendered.fovs[0];
                        const float pixelAngle =
                            (std::tan(lens.right) - std::tan(lens.left)) / static_cast<float>(targets[0].width);
                        const auto shown = saved->motion.predictedDisplayTime;
                        // Update 38.1 RING LAYER: NEWEST puts the ring and the hologram at the newest
                        // tracked hand in this image's world and hides both while this image is
                        // stale (older than the newest frame by 50 ms, or its world jumped 2 m);
                        // IMAGE is 38's (the ring by RING ANCHOR, the hologram at the image's grip).
                        // Update 38.2: HOLD (0, the default) never hides - the image's own wrist, the
                        // last good pose kept through a bad one; NEWEST is 38.1's (it hid: the flicker).
                        const bool ringNewest = c38(combat38::ringLayer) == combat38::layerNewest;
                        const float imageAge = latestShown && shown
                                                   ? static_cast<float>(static_cast<double>(latestShown - shown) * 1e-9)
                                                   : 0.f;
                        const bool ringHide =
                            ringNewest && !saved->flatScreen &&
                            combat38::ringStale(imageAge, length(latestToWorld.position - saved->worldFromTracking.position));
                        ringHiddenFrames += ringHide;
                        std::vector<Vertex> vertices;
                        // Version 0.0.72 REAL MODELS: the textured models' own vertices (hero007::setModelSink).
                        // Build 7: kept between frames (cleared, its memory reused). THOR's 50,000-triangle hammer
                        // is 150,000 vertices (6.6 MB): a new vector each frame regrew and refaulted it every frame.
                        static thread_local std::vector<Vertex> modelVerticesKept;
                        modelVerticesKept.clear();
                        std::vector<Vertex>& modelVertices = modelVerticesKept;
                        hero007::setModelSink(overlay.modelReady() ? &modelVertices : nullptr);
                        struct EndModelSink {
                            ~EndModelSink() {
                                hero007::setModelSink(nullptr);
                            }
                        } endModelSink;
                        // Where the overlay puts each tracked web shooter, and
                        // where the game's rope for this image's frame starts.
                        Vec3 shooters[2]{}, ropeStarts[2]{};
                        uint32_t shooterHands{}, ropeHands{};
                        float webGap = -1;
                        WebLook webLook;
                        webLook.radius = static_cast<float>(webRadiusTenthsMm.load()) * 1e-4f;
                        // Update 18: the style and its shade; its clock (glints, writhe,
                        // glow) the image's display time, wrapped hourly to stay precise.
                        // Update 38.5 SYMBIOTE LOOK: the symbiote webs while on; WEB STYLE itself is kept.
                        webLook.style = symbioteOn.load() ? extras385::symbiote::webStyle
                                                          : static_cast<int>(std::min<uint32_t>(webStyleNow.load(),
                                                                                                web_style::count - 1));
                        webLook.color = static_cast<int>(webShades[webLook.style].load());
                        webLook.seconds = static_cast<float>(static_cast<int64_t>(shown) % 3600000000000ll) * 1e-9f;
                        webLook.rope = c38(combat38::webRope) == 0; // Update 38 WEB ROPE
                        WebTravel webShot;
                        webShot.travel = webTravelOn.load();
                        webShot.speed = static_cast<float>(webSpeedMs.load());
                        webShot.physics = webPhysicsOn.load();
                        webShot.wiggle = static_cast<float>(webWiggle.load());
                        webShot.bend = static_cast<float>(webBend.load());
                        // Update 37: each hand's rope bend, sprung from image to image, and
                        // the web shooter's speed (the player's own motion with it).
                        static RopeBend ropeBends[2];
                        static Vec3 shooterBefore[2]{};
                        static int64_t shooterShown[2]{};
                        if (const float interval = static_cast<float>(shown - webShownBefore) * 1e-9f;
                            webShownBefore && interval > .002f && interval < .1f)
                            webFrameSeconds += (interval - webFrameSeconds) * .1f;
                        webShownBefore = shown;
                        // Update 23: drawn from the wrist's web shooter as it is (web_start
                        // wrist), or, at Update 20's palm spot, from beside the grip.
                        const unsigned startSpot = webStartSpot.load();
                        const auto fromHand = [startSpot](Vec3 start, Vec3 gripAt, Vec3 toward) {
                            return startSpot == webStartPalm ? webFromGrip(start, gripAt, toward) : start;
                        };
                        for (unsigned hand = 0; !saved->flatScreen && hand < 2; ++hand) {
                            const auto& web = saved->webs[hand];
                            const auto& timing = saved->webTimes[hand];
                            // The game's rope is part of the scene image already: the
                            // rope that draws this hand's web, which may be the other
                            // side's (one web in both hands).
                            const int slot = saved->webSlots[hand];
                            const bool gameWeb = native_webs::drawing(slot >= 0 ? static_cast<unsigned>(slot) : hand);
                            // One web in both hands: the strand comes to the other hand
                            // first, and this hand's part runs from wrist to wrist, as
                            // slack as the rope between the two holds is long.
                            const int upper = saved->strandUpper;
                            const bool lower = web.attached && upper >= 0 && upper != static_cast<int>(hand);
                            WebLine line;
                            auto grip = rendered.grips[hand];
                            grip.position += travel;
                            const auto between = [&](Vec3 start) {
                                auto other = rendered.grips[static_cast<size_t>(upper)];
                                other.position += travel;
                                const Vec3 otherShooter = webStart(other, static_cast<unsigned>(upper), startSpot);
                                // Drawn from palm to palm (Update 13); wrist to wrist (Update 23).
                                line.start = fromHand(start, grip.position, otherShooter);
                                line.end = fromHand(otherShooter, other.position, line.start);
                                line.slack = std::max(
                                    web.length - saved->webs[static_cast<size_t>(upper)].length -
                                        length(line.end - line.start),
                                    0.f);
                                line.extended = 1;
                                line.splat = false;
                            };
                            // The timeline keeps the anchor through the release, so
                            // the splat keeps its shape while the web snaps back.
                            line.seed =
                                static_cast<float>(hand) * 1.7f + timing.anchor.x * .37f + timing.anchor.z * .11f;
                            if (!rendered.swing.hands[hand].tracked) {
                                if (!gameWeb && WebAnimation::released((shown - timing.releasedAt) * 1e-9f,
                                                                       timing.wrist + travel, timing.anchor, line))
                                    appendWeb(vertices, line, viewer, pixelAngle, webLook);
                                continue;
                            }
                            const Vec3 shooter = webStart(grip, hand, startSpot);
                            shooters[hand] = shooter;
                            shooterHands |= 1u << hand;
                            if (gameWeb) {
                                // The game draws the piece between the hands
                                // itself where it can (native_webs::Held).
                                if (lower && !native_webs::drawsHeld()) {
                                    between(shooter);
                                    appendWeb(vertices, line, viewer, pixelAngle, webLook);
                                } else if (web.attached && slot == static_cast<int>(hand) &&
                                           native_eyes::renderWebStart(imageGeneration, hand, ropeStarts[hand])) {
                                    ropeHands |= 1u << hand;
                                    webGap = std::max(webGap, length(shooters[hand] - ropeStarts[hand]));
                                }
                                continue;
                            } else if (lower) {
                                between(shooter);
                                appendWeb(vertices, line, viewer, pixelAngle, webLook);
                            } else if (web.attached) {
                                line.start = fromHand(shooter, grip.position, web.anchor);
                                line.end = web.anchor;
                                line.slack = web.tension > 0 ? 0.f
                                                             : std::max(web.length - length(line.end - line.start), 0.f);
                                // Update 15: the tip flies from the palm to the
                                // anchor, the rope rippling behind it, then
                                // snaps taut and splats. The swing had the web
                                // from the start; this is the picture alone.
                                const auto shot = WebAnimation::flight(static_cast<float>(shown - timing.attachedAt) *
                                                                           1e-9f,
                                                                       length(line.end - line.start), webShot,
                                                                       webFrameSeconds);
                                line.extended = shot.extended;
                                line.wave = shot.wave;
                                line.wavePhase = shot.wavePhase;
                                line.impact = shot.impact;
                                if (shot.flying)
                                    line.slack = std::max(line.slack, length(line.end - line.start) * .0015f);
                                // Update 37: the bend (off: none, the straight Update 15 line).
                                {
                                    Vec3 velocity{};
                                    float dt = 0;
                                    if (shooterShown[hand] && static_cast<int64_t>(shown) > shooterShown[hand]) {
                                        dt = static_cast<float>(static_cast<int64_t>(shown) - shooterShown[hand]) * 1e-9f;
                                        if (dt > 0 && dt < .1f)
                                            velocity = (shooter - shooterBefore[hand]) / dt;
                                        else
                                            dt = 0;
                                    }
                                    if (static_cast<int64_t>(shown) != shooterShown[hand]) {
                                        shooterBefore[hand] = shooter;
                                        shooterShown[hand] = static_cast<int64_t>(shown);
                                    }
                                    line.bow = webShot.physics
                                                   ? ropeBends[hand].update(dt, line.end - line.start, velocity,
                                                                            web.tension, webShot.bend, shot.flying)
                                                   : Vec3{};
                                    if (!webShot.physics)
                                        ropeBends[hand].reset();
                                }
                                appendWeb(vertices, line, viewer, pixelAngle, webLook);
                            } else if (WebAnimation::released((shown - timing.releasedAt) * 1e-9f, shooter,
                                                              timing.anchor, line)) {
                                appendWeb(vertices, line, viewer, pixelAngle, webLook);
                            }
                        }
                        // Update 38.2 GRAB WEB: the rope from the web shooter to the enemy it holds.
                        for (unsigned g = 0; g < 2; ++g)
                            if (saved->grabOn[g] > 0) {
                                WebLine line{};
                                line.start = saved->grabFrom[g] + travel;
                                line.end = saved->grabTo[g] + travel;
                                line.seed = g ? .71f : .29f;
                                if (finite(line.start) && finite(line.end) && length(line.end - line.start) > .05f)
                                    appendWeb(vertices, line, viewer, pixelAngle, webLook);
                            }
                        // Version 0.0.3 WALL STICK: a web net where an enemy hit a wall or the floor (ours, not
                        // the game's web takedown), moved with the player.
                        for (size_t n = 0; n < saved->netAt.size(); ++n) {
                            if (!(saved->netFade[n] > 0))
                                continue;
                            wall_stick::Net net{};
                            net.on = true;
                            net.at = saved->netAt[n] + travel;
                            net.normal = saved->netNormal[n];
                            net.seed = saved->netSeed[n];
                            net.fade = saved->netFade[n];
                            Vec3 a[48], b[48];
                            const uint32_t c = wall_stick::strands(net, .55f * saved->netFade[n], a, b, 48);
                            for (uint32_t k = 0; k < c; ++k) {
                                WebLine line{};
                                line.start = a[k];
                                line.end = b[k];
                                line.seed = net.seed + .013f * static_cast<float>(k);
                                if (finite(line.start) && finite(line.end) && length(line.end - line.start) > .02f)
                                    appendWeb(vertices, line, viewer, pixelAngle, webLook);
                            }
                        }
                        // Version 0.0.3 AUTO PERFORMANCE SHOW STATUS: one line over the left wrist.
                        if (perfStatusShown && saved->perfAtOk > 0) {
                            const Vec3 at = saved->perfAt + travel;
                            Vec3 toViewer = viewer - at;
                            toViewer.y = 0;
                            if (finite(at) && length(toViewer) > .05f) {
                                toViewer = normalized(toViewer);
                                const Vec3 up{0, 1, 0};
                                const Vec3 right = normalized(cross(up, toViewer));
                                PixelTextStyle style;
                                style.height = .022f;
                                style.top = {.55f, 1.f, .75f};
                                style.bottom = {.25f, .8f, .55f};
                                style.alpha = 1.f;
                                style.outlineAlpha = .9f;
                                const float width = pixelTextWidth(perfStatusText, style);
                                appendPixelText(vertices, perfStatusText, at - right * (width * .5f), right, up, style);
                            }
                        }
                        // Update 38.3 GADGET HINT: the label facing the viewer, upright, moved with the player.
                        for (unsigned g = 0; g < 2; ++g) {
                            if (!(saved->hintFade[g] > 0))
                                continue;
                            const Vec3 at = saved->hintAt[g] + travel;
                            Vec3 toViewer = viewer - at;
                            toViewer.y = 0;
                            if (!finite(at) || length(toViewer) < .05f)
                                continue;
                            toViewer = normalized(toViewer);
                            const Vec3 up{0, 1, 0};
                            const Vec3 right = normalized(cross(up, toViewer));
                            PixelTextStyle style;
                            style.height = .028f;
                            style.top = {1.f, .92f, .35f};
                            style.bottom = {1.f, .55f, .12f};
                            style.alpha = saved->hintFade[g];
                            style.outlineAlpha = .9f * saved->hintFade[g];
                            const char* text = combat38::hintText(saved->hintLabel[g]);
                            const float width = pixelTextWidth(text, style);
                            appendPixelText(vertices, text, at - right * (width * .5f), right, up, style);
                        }
                        // Version 0.0.3 WEB BALL HIT: the webbed-up look (the mod's strands round a struck enemy).
                        for (const auto& c0 : saved->cocoons) {
                            if (!c0.on)
                                continue;
                            web_cocoon::Cocoon c = c0;
                            c.feet = c0.feet + travel;
                            Vec3 a[160], b[160];
                            const uint32_t n = web_cocoon::strands(c, a, b, 160);
                            for (uint32_t k = 0; k < n; ++k)
                                appendWebThread(vertices, a[k], b[k], viewer, pixelAngle, 1.f);
                        }
                        // Version 0.0.2 BALL LOOK OURS: our balls and their splashes, moved with the player.
                        // Version 0.0.4 IRON MAN ABILITIES (REPULSOR): blasts, flashes and the chest beam instead.
                        // Version 0.0.62 PLASMA FLICKER (repulsor_mcu::plasmaFlicker): the beam and bolts shimmer.
                        const float plasmaNow = static_cast<float>(std::fmod(
                            std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(), 600.0));
                        if (saved->beamFade > 0)
                            appendChestBeam(vertices, saved->beamFrom + travel, saved->beamTo + travel, viewer, pixelAngle,
                                            saved->beamFade * repulsor_mcu::plasmaFlicker(plasmaNow, 41u));
                        for (size_t m = 0; m < saved->markFade.size(); ++m) // Version 0.0.62 HIT INDICATOR
                            if (saved->markFade[m] > 0)
                                hit_marks::appendMark(vertices, saved->markAt[m] + travel, viewer, pixelAngle, saved->markFade[m]);
                        for (unsigned i = 0; i < 2; ++i) // Version 0.0.72 GAME BODY fallback gloves
                            if (saved->gloveOn[i] > 0) {
                                Pose grip = rendered.grips[i];
                                grip.position += travel;
                                glove_overlay::append(vertices, grip, i == 1, saved->gloveFist[i], viewer,
                                                      pixelAngle * length(grip.position - viewer),
                                                      heroNow() == hero_modes::batman ? glove_overlay::batDark : glove_overlay::suitRed);
                            }
                        for (unsigned i = 0; i < 2; ++i) // Version 0.0.6 DAREDEVIL's batons
                            if (saved->batonOn[i] > 0) {
                                if (saved->hero007.batonModel[i] && // Version 0.0.7 BATON LOOK MODEL
                                    hero007::appendBatonModel(vertices, saved->batonGrip[i] + travel, saved->batonTip[i] + travel,
                                                              *saved->hero007.batonModel[i], viewer, saved->batonHeld[i]))
                                    ;
                                else if (saved->batonClub) // Version 0.0.7 BATON LOOK CLUB (MODEL's fallback too)
                                    hero007::appendBatonClub(vertices, saved->batonGrip[i] + travel, saved->batonTip[i] + travel, viewer, pixelAngle);
                                else
                                    appendBaton(vertices, saved->batonGrip[i] + travel, saved->batonTip[i] + travel, viewer, pixelAngle);
                            }
                        if (saved->hero007.any && saved->hero007.beamFade > 0 && validTrackedPose(rendered.headPose)) {
                            // Version 0.0.72 HEAT VISION HEAD LOCK: the beams redrawn from this image's own head
                            // pose along where it looks (0.0.71: drawn to where the beam was aimed a frame ago).
                            hero007::Draw locked = saved->hero007;
                            hero007::headLockBeam(locked, rendered.headPose);
                            hero007::appendHero007(vertices, locked, travel, viewer, pixelAngle);
                        } else if (saved->hero007.any) // Version 0.0.7 HERO PROFILES
                            hero007::appendHero007(vertices, saved->hero007, travel, viewer, pixelAngle);
                        if (saved->hero072.any) { // Version 0.0.72 (TEST): THOR, GREEN LANTERN, IRON HUD, PERCH
                            // Build 5: THOR's look (the hammer, its chain, the storm) can never take the frame down:
                            // out of memory or a bad model drops this look for this frame only.
                            try {
                                // Test build 10: the hammer in the fist from this image's own grip pose (no lag
                                // behind the hand), a flying one carried on to this image's time.
                                const hero072::Look* look72 = &saved->hero072;
                                hero072::Look held72;
                                if (look72->hammerInHand && validTrackedPose(rendered.grips[1])) {
                                    const thor_grip::Held h = thor_grip::held(rendered.grips[1], runtime.controllers()[1]);
                                    if (h.ok) {
                                        held72 = *look72;
                                        held72.hammerAt = h.at;
                                        held72.hammerForward = h.handle;
                                        held72.hammerUp = h.face;
                                        look72 = &held72;
                                    }
                                }
                                const double now72 = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
                                const float ahead72 = look72->hammerStamp > 0 ? static_cast<float>(std::clamp(now72 - look72->hammerStamp, 0.0, .06)) : 0.f;
                                hero072::appendLook(vertices, *look72, travel, viewer, pixelAngle, ahead72);
                            } catch (...) {
                                static bool said = false;
                                if (!said)
                                    said = true, consoleLine("THOR / 0.0.72 look: not drawn this frame (out of memory?) - play goes on");
                            }
                        }
                        if (saved->shockFade > 0) // Version 0.0.5 SUPERSONIC's shockwave
                            appendShockwave(vertices, viewer, saved->shockWay, saved->shockAge, pixelAngle);
                        if (saved->beamRingOk > 0) { // Version 0.0.5 BEAM RING (right wrist)
                            Pose grip = rendered.grips[1];
                            grip.position += travel;
                            appendBeamRing(vertices, saved->beamShare, saved->beamFlash, grip, viewer, pixelAngle,
                                           static_cast<float>(std::fmod(static_cast<double>(shown) * 1e-9, 3600.)));
                        }
                        for (unsigned i = 0; i < 2; ++i) // Version 0.0.62 HAND COOLDOWN (each wrist)
                            if (saved->blastRingOk[i] > 0) {
                                Pose grip = rendered.grips[i];
                                grip.position += travel;
                                appendBlastRing(vertices, saved->blastShare[i], saved->blastFlash[i], grip, viewer, pixelAngle);
                            }
                        // Version 0.0.6 (Update 39.0.2) REPULSOR LOOK (repulsor_mcu.hpp): the palms' charge, the
                        // bolts and the impact bursts (0.0.4's bolt and flash retired).
                        for (unsigned i = 0; i < 2 && saved->repulsorLook; ++i)
                            if (saved->palmGlow[i] > 0)
                                repulsor_mcu::appendCharge(vertices, saved->palmAt[i] + travel, saved->palmWay[i], viewer, pixelAngle,
                                                           saved->palmGlow[i], saved->palmSpin);
                        for (size_t b = 0; b < saved->oursAt.size() && saved->repulsorLook; ++b) {
                            if (saved->oursFade[b] > 0)
                                repulsor_mcu::appendBolt(vertices, saved->oursAt[b] + travel, saved->oursTail[b] + travel, viewer,
                                                         pixelAngle,
                                                         saved->oursFade[b] * repulsor_mcu::plasmaFlicker(plasmaNow, static_cast<unsigned>(b)));
                            if (saved->splashFade[b] > 0)
                                repulsor_mcu::appendImpact(vertices, saved->splashAt[b] + travel, viewer, viewer, pixelAngle,
                                                           saved->splashFade[b], .2f, static_cast<unsigned>(b) + 7u);
                        }
                        for (size_t b = 0; b < saved->oursAt.size() && !saved->repulsorLook; ++b) {
                            if (saved->oursFade[b] > 0)
                                appendWebBall(vertices, saved->oursAt[b] + travel, saved->oursTail[b] + travel, viewer,
                                              pixelAngle, saved->oursFade[b]);
                            if (saved->splashFade[b] > 0)
                                appendWebSplash(vertices, saved->splashAt[b] + travel, viewer, pixelAngle,
                                                saved->splashFade[b], .37f + 1.3f * static_cast<float>(b));
                        }
                        // Version 0.0.2 TRAINING's guide: the path, its markers, arrows and ghost, and the
                        // move's name and what to do over it, facing the viewer.
                        if (saved->guide.on) {
                            appendTrainingGuide(vertices, saved->guide, travel, viewer, pixelAngle);
                            const Vec3 at = saved->guideAt + travel;
                            Vec3 toViewer = viewer - at;
                            toViewer.y = 0;
                            if (finite(at) && length(toViewer) > .05f) {
                                toViewer = normalized(toViewer);
                                const Vec3 up{0, 1, 0};
                                const Vec3 right = normalized(cross(up, toViewer));
                                const uint8_t st = saved->guide.state;
                                PixelTextStyle style;
                                style.height = .03f;
                                style.top = st == training_guide::success ? Vec3{.5f, 1.f, .6f}
                                            : st == training_guide::blue  ? Vec3{.6f, .85f, 1.f}
                                            : st == training_guide::fail  ? Vec3{1.f, .7f, .3f}
                                                                          : Vec3{1.f, .5f, .45f};
                                style.bottom = st == training_guide::success ? Vec3{.2f, .8f, .3f}
                                               : st == training_guide::blue  ? Vec3{.2f, .5f, 1.f}
                                               : st == training_guide::fail  ? Vec3{1.f, .45f, .1f}
                                                                             : Vec3{1.f, .15f, .1f};
                                style.alpha = 1;
                                style.outlineAlpha = .9f;
                                for (int l = 0; l < 2; ++l) {
                                    const char* text = saved->guideLine[l];
                                    if (!text[0])
                                        continue;
                                    const float width = pixelTextWidth(text, style);
                                    appendPixelText(vertices, text, at - right * (width * .5f) - up * (static_cast<float>(l) * .045f), right, up,
                                                    style);
                                }
                            }
                        }
                        // Update 38 BALL LOOK GLOB: the web-ball globs, moved with the player to this image.
                        for (unsigned g = 0; g < 2; ++g)
                            if (saved->globFade[g] > 0)
                                appendWebBall(vertices, saved->globAt[g] + travel, saved->globTail[g] + travel, viewer,
                                              pixelAngle, saved->globFade[g]);
                        // Aim markers: where each free hand's web button would send its
                        // web, on the line that hand points along in this image, that
                        // line steadied from image to image. Translucent, the farthest
                        // drawn first.
                        std::vector<AimMarker> marks;
                        for (unsigned hand = 0; hand < 2; ++hand) {
                            auto& steady = aimMotion[hand];
                            const auto& input = rendered.swing.hands[hand];
                            if (saved->flatScreen) {
                                steady.reset();
                                continue;
                            }
                            steady.begin(shown);
                            if (!input.tracked) {
                                steady.markers(nullptr, {}, {}, marks);
                                continue;
                            }
                            const auto& aim = saved->aims[hand];
                            const auto kind = static_cast<game_swing::AimKind>(aim.kind);
                            // The hand moved with the player to this image, as its glove does.
                            const Vec3 origin = input.aim.position + travel;
                            const Vec3 direction =
                                steady.aim(normalized(input.aim.orientation.rotate({0, 0, -1})),
                                           trackingTurn(rendered.swing));
                            const float reach = length(aim.point - input.aim.position);
                            AimMarker marker;
                            marker.squeeze = input.grip;
                            marker.hand = hand;
                            // A hand pointed at one of the game's interactions on
                            // Spidy's wall shows that instead: a turning ring round
                            // one the interact would use, a faint one round one to
                            // come closer to.
                            if (const auto& offer = saved->offers[hand]; offer.handle) {
                                marker.kind = offer.usable ? AimMark::target : AimMark::air;
                                marker.point = offer.position;
                                marker.radius = offer_pointing::markRadius;
                                steady.markers(&marker, origin, direction, marks);
                                ++markersDrawn;
                                continue;
                            }
                            switch (kind) {
                            case game_swing::AimKind::prop:
                            case game_swing::AimKind::character:
                                marker.kind = AimMark::target;
                                marker.point = aim.point;
                                marker.radius = aim.radius;
                                break;
                            case game_swing::AimKind::anchor:
                                marker.point = onAimLine(aim.point, aim.normal, origin, direction);
                                break;
                            case game_swing::AimKind::air:
                                marker.kind = AimMark::air;
                                marker.point = origin + direction * reach;
                                break;
                            default:
                                marker.kind = AimMark::blocked;
                                marker.point = length(aim.normal) > .5f
                                                   ? onAimLine(aim.point, aim.normal, origin, direction)
                                                   : origin + direction * reach;
                                break;
                            }
                            // Not on the floor a lowered hand points at, by the player's feet.
                            const float away = length(marker.point - origin);
                            const float nearest = marker.kind == AimMark::target    ? .5f
                                                  : marker.kind == AimMark::blocked ? 3.f
                                                                                    : 2.f;
                            const bool wanted = kind != game_swing::AimKind::none &&
                                                kind <= game_swing::AimKind::character &&
                                                !saved->webs[hand].attached && away >= nearest;
                            steady.markers(wanted ? &marker : nullptr, origin, direction, marks);
                            markersDrawn += wanted;
                        }
                        std::sort(marks.begin(), marks.end(), [&](const AimMarker& a, const AimMarker& b) {
                            return length(a.point - viewer) > length(b.point - viewer);
                        });
                        std::vector<Vertex> translucent;
                        // Update 36: drawn with the art texture (calibration_panel.hpp).
                        std::vector<Vertex> art;
                        bool plainPanel = false;
                        // Update 16: reticles off hides only these drawings; the
                        // markers' motion above still runs, so turning them back on
                        // shows them where they belong at once.
                        // Version 0.0.72 SUBTLE RETICLE (captain.hpp): CAPTAIN AMERICA's markers small and faint.
                        if (heroNow() == hero_modes::captainAmerica)
                            for (auto& mark : marks) {
                                if (mark.kind == AimMark::target) {
                                    mark.radius = captain::subtleRadius(mark.radius);
                                    mark.lock = 1; // no wide lock-on ring
                                }
                                mark.opacity *= captain::reticleOpacity;
                            }
                        if (reticlesShown.load())
                            for (const auto& mark : marks)
                                appendAimMarker(translucent, mark, viewer, pixelAngle * markerPixels,
                                                saved->uiStyle);
                        // Slow motion's focus meter over the back of the left wrist, moved
                        // with the player to this image as the glove is. Update 36: in
                        // the comic style with the art loaded, the health and focus ring
                        // under the wrist instead, its lower crescent the meter (shown
                        // the same way: the same test, the same fade).
                        if (!saved->flatScreen && !ringHide && saved->slow.meter > 0 && rendered.swing.hands[0].tracked) {
                            // Update 38 RING ANCHOR: HAND the newest tracked wrist in this image's
                            // world (the image may be reused, the hand moved on), WORLD 37.2's.
                            float ringGap = 0;
                            // Update 38.2: ringWrist's legacy modes are 0 HAND, 1 WORLD.
                            const uint32_t anchor = c38(combat38::ringAnchor);
                            auto wrist = combat38::ringWrist(rendered.hands[0], saved->worldFromTracking, latestToWorld,
                                                             latestWrist,
                                                             !latestWristTracked ? 1u
                                                             : ringNewest        ? 0u
                                                             : anchor == combat38::anchorHand ? 0u
                                                                                              : 1u,
                                                             &ringGap);
                            ringErrorNow = ringGap;
                            ringErrorPeak = std::max(ringErrorPeak, ringGap);
                            wrist.position += travel;
                            // Update 38.2 RING ANCHOR WRIST (0, the default): the ring's centre on the
                            // drawn wrist's web shooter - the IMAGE's grip (in step with the glove in the
                            // picture), pulled back toward the shoulder by the body's reach shortfall.
                            const bool atWrist = anchor == combat38::anchorWrist && !ringNewest &&
                                                 validTrackedPose(rendered.grips[0]) && validTrackedPose(rendered.headPose);
                            Vec3 wristSpot{};
                            if (atWrist) {
                                Pose grip = rendered.grips[0];
                                grip.position += travel;
                                Pose head = rendered.headPose;
                                head.position += travel;
                                float shortfall = 0;
                                if (native_body::drawn()) {
                                    const float e = native_body::status().handError[0];
                                    shortfall = std::isfinite(e) && e > 0 ? e : 0.f;
                                }
                                wristSpot = combat38::drawnWrist(combat38::webShooterSpot(grip, 0),
                                                                 combat38::shoulderOf(head, 0), shortfall);
                                wrist.orientation = grip.orientation * combat38::gripToAim();
                            }
                            const double clock = std::fmod(static_cast<double>(shown) * 1e-9, 3600.);
                            if (saved->uiStyle != UiStyle::tech && overlay.artReady())
                            {
                                // Update 37: flat on the web shooter under the wrist, faded
                                // as it turns away (ring_face 1), or Update 36's (0, plan B).
                                health_ring::Place place;
                                place.flat = ringFlat.load();
                                place.offsetMm = static_cast<float>(ringOffsetMm.load());
                                place.tiltDegrees = static_cast<float>(ringTiltDegrees.load());
                                if (atWrist && finite(wristSpot)) {
                                    // Its centre where health_ring puts it from this pose, moved onto the spot.
                                    const auto at = health_ring::placed(wrist, place, viewer);
                                    if (at.valid && finite(at.centre))
                                        wrist.position += wristSpot - at.centre;
                                }
                                if (c38(combat38::ringLayer) == combat38::layerHold) {
                                    wrist = ringHold.update(wrist, finite(wrist.position));
                                    if (atWrist && finite(wristSpot)) {
                                        const auto at = health_ring::placed(wrist, place, viewer);
                                        ringWristNow = at.valid ? length(at.centre - wristSpot) : 0.f;
                                        ringWristPeak = std::max(ringWristPeak, ringWristNow);
                                    }
                                }
                                health_ring::append(art, saved->slow, saved->health, wrist, place, viewer, pixelAngle,
                                                    static_cast<float>(clock));
                            }
                            else
                                appendSlowMotionMeter(translucent, saved->slow, wrist, viewer, pixelAngle,
                                                      static_cast<float>(clock), saved->uiStyle);
                        }
                        // Update 37: the MIRROR - nothing at all while off (no state touched,
                        // no vertex). Placed once (RECENTRE), then fixed in the playspace or
                        // the world, never following the gaze; its cost measured here.
                        // Update 38.3 FAR WINDOWS: its percent to the view gates when it changes (and at
                        // the first frame); refused (4402) only outside 100-300, which these never are.
                        // Version 0.0.3 AUTO PERFORMANCE (EXPERIMENTAL, OFF by default; auto_perf.hpp): on each
                        // new eye pair the controller steps a ladder of live knobs (FAR WINDOWS, SHARED LOD, FAR
                        // SHADOWS), never above the user's own values; OFF puts the user's values back exactly.
                        {
                            const bool perfOn = c38(combat38::autoPerf) == 1;
                            const double perfNow = std::chrono::duration<double>(
                                                       std::chrono::steady_clock::now().time_since_epoch()).count();
                            auto readUser = [&] {
                                auto_perf::Knobs k;
                                k.farWindowsPercent = combat38::farWindowsPercentOf(c38(combat38::farWindows));
                                k.sharedLod = std::min<uint32_t>(shared_lod::modeOf(SpidySharedLodNow()), 2u);
                                k.farShadows = std::min<uint32_t>(far_shadows::modeOf(SpidyFarShadowsNow()), 2u);
                                return k;
                            };
                            if (perfOn && !perfWas) { // the user's values: the ceiling, and what OFF restores
                                perfUser = readUser();
                                autoPerf.setUser(perfUser);
                                perfApplied = perfWant = perfUser;
                                perfRestoring = false;
                                perfPairAt = 0;
                            }
                            if (perfOn) {
                                // The user changed a knob while ON (the hologram or live.txt): his new value.
                                const auto live = readUser();
                                bool changed = live.farWindowsPercent != perfUser.farWindowsPercent;
                                if (perfApplied.sharedLod == perfWant.sharedLod && live.sharedLod != perfApplied.sharedLod)
                                    changed = true;
                                if (perfApplied.farShadows == perfWant.farShadows && live.farShadows != perfApplied.farShadows)
                                    changed = true; // also FAR SHADOWS' own FPS guard turning it off
                                if (changed) {
                                    perfUser.farWindowsPercent = live.farWindowsPercent;
                                    if (perfApplied.sharedLod == perfWant.sharedLod && live.sharedLod != perfApplied.sharedLod)
                                        perfUser.sharedLod = live.sharedLod;
                                    if (perfApplied.farShadows == perfWant.farShadows &&
                                        live.farShadows != perfApplied.farShadows)
                                        perfUser.farShadows = live.farShadows;
                                    perfApplied.sharedLod = live.sharedLod;
                                    perfApplied.farShadows = live.farShadows;
                                    autoPerf.setUser(perfUser);
                                    perfWant = autoPerf.now();
                                    ++perfUserChanged;
                                }
                            }
                            const float headsetHz = wristFps.refreshHz();
                            const float target = perfPace.pick(c38(combat38::perfTarget), headsetHz,
                                                               perfOn ? autoPerf.meanFps() : 0.f);
                            if (perfNewPair || perfOn != perfWas) {
                                const float pairMs = perfPairAt > 0 ? static_cast<float>((perfPairSeconds - perfPairAt) * 1000.0)
                                                                    : 0.f;
                                perfPairAt = perfPairSeconds;
                                const uint32_t zone = auto_perf::zoneOf(
                                    perfHeroX.load(std::memory_order_relaxed), perfHeroZ.load(std::memory_order_relaxed),
                                    auto_perf::activityOf(perfEnemyMetres.load(std::memory_order_relaxed),
                                                          perfHeroSpeed.load(std::memory_order_relaxed)));
                                perfZoneNow = zone;
                                const uint32_t eventsWas = autoPerf.eventCount();
                                if (autoPerf.frame(perfOn, pairMs / 1000.f, pairMs, target, zone, c38(combat38::perfFloor)))
                                    perfWant = perfOn ? autoPerf.now() : perfUser;
                                for (uint32_t e = eventsWas; e < autoPerf.eventCount(); ++e) { // every adjustment
                                    const auto& ev = autoPerf.log()[e % auto_perf::Controller::events];
                                    char line[160];
                                    std::snprintf(line, sizeof line, "AUTO PERF: %s -> %s (%s, %.1f FPS, zone %08x)\n",
                                                  auto_perf::rungNames[ev.from % auto_perf::rungs],
                                                  auto_perf::rungNames[ev.to % auto_perf::rungs],
                                                  auto_perf::whyNames[ev.why % auto_perf::whys], ev.fps, ev.zone);
                                    OutputDebugStringA(line);
                                }
                                perfNewPair = false;
                            }
                            if (!perfOn && perfWas) { // OFF: the user's values back, exactly
                                perfWant = perfUser;
                                perfRestoring = true;
                                ++perfRestores;
                            }
                            // Apply the wanted knobs (a refusal is retried each second until it takes).
                            if ((perfOn || perfRestoring) && perfNow >= perfRetryAt) {
                                bool failed = false;
                                if (perfWant.sharedLod != perfApplied.sharedLod) {
                                    if (SpidySharedLod(reinterpret_cast<void*>(uintptr_t{perfWant.sharedLod})) == 0)
                                        perfApplied.sharedLod = perfWant.sharedLod;
                                    else
                                        failed = true;
                                }
                                if (perfWant.farShadows != perfApplied.farShadows) {
                                    if (SpidyFarShadows(reinterpret_cast<void*>(uintptr_t{perfWant.farShadows})) == 0)
                                        perfApplied.farShadows = perfWant.farShadows;
                                    else
                                        failed = true;
                                }
                                if (failed) {
                                    ++perfRefused;
                                    perfRetryAt = perfNow + 1.0;
                                }
                                if (!perfOn && perfApplied.sharedLod == perfWant.sharedLod &&
                                    perfApplied.farShadows == perfWant.farShadows)
                                    perfRestoring = false;
                            }
                            perfWas = perfOn;
                            // Update 38.3 FAR WINDOWS: its percent to the view gates when it changes (and at the
                            // first frame); with AUTO PERFORMANCE ON, the ladder's (never above the user's).
                            const uint32_t farWant = perfOn ? std::min(perfWant.farWindowsPercent,
                                                                       combat38::farWindowsPercentOf(c38(combat38::farWindows)))
                                                            : combat38::farWindowsPercentOf(c38(combat38::farWindows));
                            if (farWant != farAppliedPercent) {
                                farAppliedPercent = farWant;
                                SpidyFarWindows(reinterpret_cast<void*>(static_cast<uintptr_t>(farWant)));
                            }
                            perfStatusShown = perfOn && c38(combat38::perfStatus) == 0;
                            if (perfStatusShown)
                                auto_perf::status(perfStatusText, sizeof perfStatusText, autoPerf, target);
                            if (perfNow - perfReportAt >= .25 || perfNow < perfReportAt) {
                                perfReportAt = perfNow;
                                std::lock_guard<std::mutex> lock(perfReportLock);
                                auto& r = perfReport;
                                r.on = perfOn;
                                r.rung = autoPerf.rung();
                                r.zone = perfZoneNow;
                                r.eventCount = autoPerf.eventCount();
                                r.target = target;
                                r.meanFps = autoPerf.meanFps();
                                r.missed = autoPerf.missedShare();
                                r.headsetHz = headsetHz;
                                r.userFar = perfUser.farWindowsPercent;
                                r.userLod = perfUser.sharedLod;
                                r.userShadows = perfUser.farShadows;
                                r.nowFar = farAppliedPercent;
                                r.nowLod = perfApplied.sharedLod;
                                r.nowShadows = perfApplied.farShadows;
                                r.refused = perfRefused;
                                r.userChanged = perfUserChanged;
                                r.drops = autoPerf.drops();
                                r.raises = autoPerf.raises();
                                r.zoneJumps = autoPerf.zoneJumps();
                                r.bans = autoPerf.bans();
                                r.restores = perfRestores;
                                const uint32_t count = autoPerf.eventCount();
                                const uint32_t first = count > v003::perfEvents ? count - v003::perfEvents : 0;
                                for (uint32_t e = 0; e < v003::perfEvents; ++e)
                                    r.log[e] = first + e < count ? autoPerf.log()[(first + e) % auto_perf::Controller::events]
                                                                 : auto_perf::Event{};
                            }
                        }
                        if (validTrackedPose(rendered.headPose)) // Update 37: the WORLD LOG's gaze (3 stores)
                            world_trace::look(rendered.headPose.orientation.rotate(Vec3{0, 0, -1}));
                        mirrorInSight = false; // set again below while the mirror is drawn
                        // Build 9 SUITS: the picked suit's model on the player - skinned once from the headset
                        // and the grips (suit_wear::estimate), its SUIT BODY part drawn in first person now and
                        // the whole suit reflected in the mirror below (in place of the FIGURE). Textured into
                        // the model sink; any failure: nothing drawn, never a crash.
                        static suit_wear::Skinned suitSkinned;
                        static float suitStanding = 0, suitMean = 0;
                        // Test build 10 MIRROR VIEW: the frame budget (mirror_guard.hpp) - at 45 Hz every other
                        // frame reuses the last reflection (kept with the travel and suit it was drawn at).
                        static mirror_guard::Guard mirrorGuard;
                        static std::vector<Vertex> mirrorKept;
                        static Vec3 mirrorKeptTravel{};
                        static uint32_t mirrorKeptSlot = 0;
                        if (!mirrorOn.load())
                            mirrorGuard = {}, mirrorKept.clear(), mirrorHalfRate = false;
                        const bool mirrorReuse = mirrorOn.load() && mirror_guard::reuse(mirrorGuard) && !mirrorKept.empty() &&
                                                 mirrorKeptSlot == wornSuitSlot.load();
                        const mesh_model::Mesh* suitMesh = nullptr;
                        const suit_wear::Rig* suitRig = nullptr;
                        bool suitWorn = false;
                        if (const uint32_t ws = wornSuitSlot.load(); !saved->flatScreen && mesh_model::slotIsSuit(ws) &&
                                                                     overlay.modelReady() && validTrackedPose(rendered.headPose)) {
                            const auto suitStart = std::chrono::steady_clock::now();
                            try {
                                suitMesh = mesh_model::library().model(ws);
                                suitRig = &suitRigs[ws - mesh_model::slotSuitFirst];
                                if (suitMesh && suitRig->ok) {
                                    suit_wear::Tracked t;
                                    const Quat turn = trackingTurn(rendered.swing);
                                    t.up = normalized(turn.rotate(Vec3{0, 1, 0}));
                                    t.feet = rendered.anchor + travel;
                                    t.eye = rendered.headPose.position + travel;
                                    t.eyeForward = rendered.headPose.orientation.rotate(Vec3{0, 0, -1});
                                    t.eyeUp = rendered.headPose.orientation.rotate(Vec3{0, 1, 0});
                                    for (unsigned i = 0; i < 2; ++i) {
                                        t.hand[i] = rendered.swing.hands[i].tracked && validTrackedPose(rendered.grips[i]);
                                        t.grip[i] = rendered.grips[i].position + travel;
                                        t.gripForward[i] = rendered.grips[i].orientation.rotate(Vec3{0, 0, -1});
                                        t.gripX[i] = rendered.grips[i].orientation.rotate(Vec3{1, 0, 0});
                                        // Test build 12 HANDS: the forearm through the fist (the controller's handle
                                        // tilt) and the FIST: the trigger or the grip held, or a prop in this hand
                                        t.gripUp[i] = rendered.grips[i].orientation.rotate(Vec3{0, 1, 0});
                                        t.handleTiltDeg[i] = baton_grip::handleTiltDeg(runtime.controllers()[i]);
                                        const auto& sh = rendered.swing.hands[i];
                                        float want = std::max({sh.trigger, sh.grip, sh.wallGrip});
                                        if (saved->batonHeld[i] || (i == 1 && saved->hero072.hammerInHand))
                                            want = 1.f;
                                        static float curlNow[2]{};
                                        want = std::isfinite(want) ? std::clamp(want, 0.f, 1.f) : 0.f;
                                        curlNow[i] += (want - curlNow[i]) * .35f;
                                        t.curl[i] = curlNow[i];
                                    }
                                    // The standing eye height: the highest lately (a crouch does not shrink the suit).
                                    const float eyeUp = dot(t.eye - t.feet, t.up);
                                    if (std::isfinite(eyeUp) && eyeUp > .8f && eyeUp < 2.4f)
                                        suitStanding = eyeUp > suitStanding ? eyeUp : suitStanding + (eyeUp - suitStanding) * .0004f;
                                    t.standingEye = suitStanding;
                                    suit_wear::Skeleton live;
                                    suit_wear::BoneMove mv[suit_wear::bones];
                                    // Test build 10: BODY on - the suit's FULL BODY in the game body's place; BATMAN never OFF.
                                    const uint32_t mask = suit_wear::partBones(suit_wear::firstPersonPart(
                                        suit_wear::partOf(c38(combat38::suitBody)), hero_modes::gameBodyShown(heroNow()), saved->bodyWanted));
                                    // skinned only when something shows it (SUIT BODY on, or the mirror up): else no cost
                                    suitWorn = (mask || (mirrorOn.load() && !mirrorReuse)) && suit_wear::estimate(*suitRig, t, live) &&
                                               suit_wear::moves(*suitRig, live, mv) && suit_wear::skin(*suitRig, mv, suitSkinned);
                                    // Test build 12 CAPE: the cape hangs from the torso, and fans out while gliding (eased:
                                    // about a fifth of a second in, a third out)
                                    static float capeSpread = 0;
                                    {
                                        const bool gl = capeGlidingNow.load(std::memory_order_relaxed);
                                        capeSpread += ((gl ? 1.f : 0.f) - capeSpread) * (gl ? .12f : .07f);
                                        if (!std::isfinite(capeSpread))
                                            capeSpread = 0;
                                    }
                                    if (suitWorn && suitRig->hasCape)
                                        suit_wear::cape(*suitRig, mv, live, capeSpread, suitSkinned);
                                    // Test build 10 ONE BODY: drawn, it stands in for the game's body (suitShownAtMs).
                                    if (suitWorn && mask &&
                                        suit_wear::draw(modelVertices, hero007::modelSinkBudget, *suitMesh, *suitRig, suitSkinned, mask))
                                        suitShownAtMs.store(GetTickCount64(), std::memory_order_relaxed);
                                }
                            } catch (...) {
                                suitWorn = false;
                            }
                            const float us = std::chrono::duration<float, std::micro>(std::chrono::steady_clock::now() - suitStart).count();
                            suitMean = suitMean > 0 ? suitMean * .98f + us * .02f : us;
                            suitWearMicros = static_cast<uint32_t>(suitMean + .5f);
                        }
                        if (!saved->flatScreen && mirrorOn.load() && validTrackedPose(rendered.headPose)) {
                            static vr_mirror::Placement mirrorPlace;
                            static float mirrorMean{}, mirrorPeak{};
                            const auto mirrorStart = std::chrono::steady_clock::now();
                            try {
                                vr_mirror::Config mc;
                                mc.on = true;
                                mc.distanceCm = mirrorDistanceCm.load();
                                mc.sizePercent = mirrorSizePercent.load();
                                mc.spot = mirrorSpot.load();
                                mc.anchor = mirrorAnchor.load();
                                mc.style = mirrorStyle.load();
                                vr_mirror::Frame mf;
                                mf.anchor = rendered.anchor + travel;
                                mf.turn = trackingTurn(rendered.swing);
                                mf.head = rendered.headPose;
                                mf.head.position += travel;
                                for (unsigned i = 0; i < 2; ++i) {
                                    mf.handTracked[i] =
                                        rendered.swing.hands[i].tracked && validTrackedPose(rendered.grips[i]);
                                    mf.hands[i] = rendered.grips[i];
                                    mf.hands[i].position += travel;
                                }
                                // Update 38.2 MIRROR LOOK SUIT (0, the default): the hero's own solved
                                // skeleton (native_body::suit, fresh within 250 ms) drawn in the suit's
                                // colours; FIGURE (1, plan B) or no solve: 37's figure.
                                mf.suit = false;
                                mf.palette = suitPalette;
                                if (symbioteOn.load()) { // Update 38.5 SYMBIOTE LOOK: the black suit in the mirror
                                    mf.palette.primary = extras385::symbiote::primary;
                                    mf.palette.secondary = extras385::symbiote::secondary;
                                    mf.palette.emblem = extras385::symbiote::emblem;
                                }
                                if (c38(combat38::mirrorLook) == 0 && native_body::drawn()) {
                                    native_body::Suit suit;
                                    if (native_body::suit(suit) && GetTickCount64() - suit.solvedAt < 250) {
                                        const Vec3 feet = rendered.anchor + travel;
                                        mf.suit = true;
                                        for (uint32_t j = 0; j < native_body::suitJoints; ++j)
                                            mf.joints[j] = feet + suit.joints[j];
                                        mf.suit = vr_mirror::suitUsable(mf);
                                    }
                                }
                                mirrorSuitFrames += mf.suit;
                                mirrorJointsLast = mf.suit ? native_body::suitJoints : 0u;
                                bool recentred{};
                                if (mirrorRecentreAsked.exchange(false) || !mirrorPlace.placed ||
                                    mirrorPlace.anchor != mc.anchor) {
                                    mirrorPlace = vr_mirror::recentre(mf, mc.anchor);
                                    recentred = mirrorPlace.placed;
                                }
                                const auto plane = vr_mirror::plane(mirrorPlace, mf, mc);
                                // Update 38.2: in view (its centre within 35 degrees of the gaze, 4 m): no attack.
                                mirrorInView = mirrorInSight = false;
                                if (plane.valid) {
                                    const Vec3 to = plane.centre - mf.head.position;
                                    const float d = length(to);
                                    const Vec3 gaze = mf.head.orientation.rotate(Vec3{0, 0, -1});
                                    mirrorInView = d > .1f && d < 4.f && dot(to / d, gaze) > std::cos(35.f * .0174532925f);
                                    mirrorInSight = d > .1f && dot(to / d, gaze) > std::cos(70.f * .0174532925f);
                                }
                                // Build 9 SUITS: the worn suit's whole model reflected (the frame drawn, not the FIGURE).
                                mf.frameOnly = false;
                                if (mirrorReuse && plane.valid && mirrorInSight &&
                                    modelVertices.size() + mirrorKept.size() <= hero007::modelSinkBudget) {
                                    // 45 Hz: the last reflection, moved as the player moved (a WORLD mirror reflects the move).
                                    Vec3 d = travel - mirrorKeptTravel;
                                    if (mc.anchor == vr_mirror::world)
                                        d = d - plane.normal * (2.f * dot(d, plane.normal));
                                    const size_t before = modelVertices.size();
                                    modelVertices.insert(modelVertices.end(), mirrorKept.begin(), mirrorKept.end());
                                    for (size_t v = before; v < modelVertices.size(); ++v)
                                        modelVertices[v].position += d;
                                    mf.frameOnly = true;
                                } else if (suitWorn && plane.valid && mirrorInSight) { // out of sight: the FIGURE (build 7's cost)
                                    const Vec3 n = plane.normal, c = plane.centre;
                                    const float s = plane.scale;
                                    float place[12];
                                    const float nn[3] = {n.x, n.y, n.z}, cc[3] = {c.x, c.y, c.z};
                                    for (int r = 0; r < 3; ++r) {
                                        float t = cc[r];
                                        for (int q = 0; q < 3; ++q) {
                                            const float m = s * ((r == q ? 1.f : 0.f) - 2.f * nn[r] * nn[q]);
                                            place[r * 4 + q] = m;
                                            t -= m * cc[q];
                                        }
                                        place[r * 4 + 3] = t;
                                    }
                                    const auto reflStart = std::chrono::steady_clock::now();
                                    const size_t before = modelVertices.size();
                                    mf.frameOnly = suit_wear::draw(modelVertices, hero007::modelSinkBudget, *suitMesh, *suitRig, suitSkinned,
                                                                   suit_wear::allBones, place);
                                    suitWearVertices = static_cast<uint32_t>(modelVertices.size() - before);
                                    if (mf.frameOnly && mirrorGuard.half) // kept for the next (reused) frame
                                        mirrorKept.assign(modelVertices.begin() + static_cast<std::ptrdiff_t>(before), modelVertices.end()),
                                            mirrorKeptTravel = travel, mirrorKeptSlot = wornSuitSlot.load();
                                    else
                                        mirrorKept.clear();
                                    const float us = std::chrono::duration<float, std::micro>(std::chrono::steady_clock::now() - reflStart).count();
                                    suitMean += us * .02f;
                                }
                                const size_t mirrorMade = vr_mirror::append(translucent, plane, mf, mc, viewer);
                                const float ms = std::chrono::duration<float, std::milli>(
                                                     std::chrono::steady_clock::now() - mirrorStart)
                                                     .count();
                                mirrorMean = mirrorMean > 0 ? mirrorMean * .95f + ms * .05f : ms;
                                mirrorPeak = std::max(mirrorPeak * .999f, ms);
                                mirrorMicros = static_cast<uint32_t>(mirrorMean * 1000.f + .5f);
                                // Test build 10 MIRROR VIEW: over budget - 45 Hz, then OFF with the wrist's note.
                                {
                                    static auto guardLast = std::chrono::steady_clock::now();
                                    const auto guardNow = std::chrono::steady_clock::now();
                                    const float dt = std::chrono::duration<float>(guardNow - guardLast).count();
                                    guardLast = guardNow;
                                    if (mirror_guard::step(mirrorGuard, ms, dt)) {
                                        mirrorOn = false;
                                        mirrorAutoOffMicros = std::max<uint32_t>(1, static_cast<uint32_t>(mirrorGuard.meanMs * 1000.f + .5f));
                                        mirrorAutoOffAtMs = GetTickCount64();
                                        game_prompts::note("Test build 10: the mirror was too slow and turned itself off");
                                    }
                                    mirrorHalfRate = mirrorGuard.half;
                                }
                                publish(SpidyMirrorData, [&](vr_mirror::Data& d) {
                                    d.on = 1;
                                    d.anchor = mc.anchor;
                                    d.spot = mc.spot;
                                    d.style = mc.style;
                                    d.recentres += recentred ? 1 : 0;
                                    ++d.frames;
                                    d.vertices = mirrorMade;
                                    d.lastMs = ms;
                                    d.meanMs = mirrorMean;
                                    d.peakMs = mirrorPeak;
                                    d.distance = static_cast<float>(mc.distanceCm) / 100.f;
                                });
                            } catch (...) {
                                // Never the flat screen, never a crash: the mirror turns itself off.
                                mirrorOn = false;
                                mirrorInView = mirrorInSight = false;
                                mirrorAutoOffMicros = 0;
                                mirrorAutoOffAtMs = GetTickCount64();
                                game_prompts::note("Update 37: the mirror failed and turned itself off");
                            }
                        }
                        // Update 37: BLOOD SPLATTER (OFF: nothing kept, nothing drawn).
                        bloodViewer = viewer;
                        if (const uint32_t blood = bloodIntensity.load(); blood || bloodPool.any()) {
                            const auto bloodStart = std::chrono::steady_clock::now();
                            try {
                                const float dt = bloodLast ? static_cast<float>(static_cast<double>(shown - bloodLast) * 1e-9)
                                                           : 0.f;
                                bloodLast = shown;
                                bloodPool.step(std::clamp(dt, 0.f, .1f));
                                // Update 38.2 BLOOD LOOK: BRUTAL (0, the default), BIG (38.1's), 37 SMALL.
                                const bool extremeBlood = blood_splatter::isExtreme(blood); // Update 38.5
                                const bool brutalBlood = extremeBlood || c38(combat38::bloodLook) == combat38::bloodBrutal;
                                const bool bigBlood = brutalBlood || c38(combat38::bloodLook) == combat38::bloodBig;
                                bloodVisorDrops.step(std::clamp(dt, 0.f, .1f));
                                for (unsigned i = 0; i < 2; ++i)
                                    if (bloodPending & (1u << i) && validTrackedPose(rendered.grips[i])) {
                                        // Update 38.1: the punch's AIM AXIS ray (38: the grip's -Z).
                                        const Vec3 aim = length(bloodAim[i]) > .5f ? normalized(bloodAim[i])
                                                                                   : rendered.grips[i].orientation.rotate(Vec3{0, 0, -1});
                                        const Vec3 at = bigBlood && finite(bloodAt[i]) ? bloodAt[i] + travel - aim * .15f
                                                                                       : rendered.grips[i].position + travel + aim * .3f;
                                        if (bigBlood && finite(bloodAt[i]))
                                            bloodPool.fire(at, aim, blood, false, true, bloodFloor[i] + travel.y, brutalBlood);
                                        else
                                            bloodPool.fire(at, aim, blood, false, bigBlood, NAN, brutalBlood);
                                        // Update 38.5 EXTREME + VISOR DROPS: a burst close to the face.
                                        if (extremeBlood && bloodVisor.load() && length(at - viewer) < blood_splatter::visorMetres)
                                            bloodVisorDrops.splat(static_cast<uint32_t>(bloodPool.fired * 2246822519u));
                                    }
                                bloodPending = 0;
                                if (!extremeBlood || !bloodVisor.load())
                                    bloodVisorDrops.age = -1;
                                if (!blood)
                                    bloodPool.clear();
                                else if (!saved->flatScreen) {
                                    if (bloodVisorDrops.age >= 0 && validTrackedPose(rendered.headPose)) {
                                        const Quat q = rendered.headPose.orientation;
                                        blood_splatter::appendVisor(translucent, bloodVisorDrops, viewer, q.rotate(Vec3{0, 0, -1}),
                                                                    q.rotate(Vec3{0, 1, 0}), q.rotate(Vec3{1, 0, 0}));
                                    }
                                    blood_splatter::append(translucent, bloodPool, viewer);
                                    // Update 38.2 BRUTAL: the coat on each glove, fading over 12 s.
                                    for (unsigned i = 0; i < 2; ++i) {
                                        bloodCoats[i].update(std::clamp(dt, 0.f, .1f));
                                        if (brutalBlood && bloodCoats[i].amount > 0 && rendered.swing.hands[i].tracked &&
                                            validTrackedPose(rendered.grips[i])) {
                                            const Vec3 aim = rendered.grips[i].orientation.rotate(Vec3{0, 0, -1});
                                            const Vec3 fist = rendered.grips[i].position + travel + aim * .03f;
                                            blood_splatter::appendCoat(translucent, fist, aim, bloodCoats[i].amount,
                                                                       0x51u + i * 977u, viewer, extremeBlood,
                                                                       static_cast<float>(static_cast<double>(shown % 1000000000000ll) * 1e-9));
                                        }
                                    }
                                }
                            } catch (...) {
                                bloodIntensity = 0; // never a crash: BLOOD SPLATTER turns itself off
                                bloodPool.clear();
                                bloodCoats[0] = {};
                                bloodCoats[1] = {};
                                bloodVisorDrops = {};
                                game_prompts::note("Update 37: blood splatter failed and turned itself off");
                            }
                            // Update 38.5 SYMBIOTE LOOK: the goo over each glove (our overlay), spreading
                            // in 0.8 s; a failure turns the look off, never the game.
                            try {
                                if (symbioteSpread.amount > 0 && !saved->flatScreen)
                                    for (unsigned i = 0; i < 2; ++i)
                                        if (rendered.swing.hands[i].tracked && validTrackedPose(rendered.grips[i])) {
                                            const Vec3 aim = rendered.grips[i].orientation.rotate(Vec3{0, 0, -1});
                                            extras385::symbiote::appendGlove(
                                                translucent, rendered.grips[i].position + travel + aim * .03f, aim,
                                                symbioteSpread.amount, 0x5e3bu + i * 131u, viewer,
                                                static_cast<float>(static_cast<double>(shown % 1000000000000ll) * 1e-9));
                                        }
                            } catch (...) {
                                symbioteOn = false;
                                symbioteSpread = {};
                                game_prompts::note("Update 38.5: SYMBIOTE LOOK's goo failed and turned itself off");
                            }
                            // Update 38.5 FLY MODE's VIGNETTE: the edge of the view darkens with the
                            // flight's speed (extras385::fly::Data::vignette), a ring 0.3 m ahead.
                            // FLY ROLL COMFORT forces it while rolled, with a level horizon line 1.5 m ahead.
                            const bool rollComfort = flyRollNow.load() == extras385::fly::rollComfort &&
                                                     flyRollState.comfort() > .01f;
                            const float flyShade = std::max(flyVignetteOn.load() && std::isfinite(flyNow.vignette)
                                                                ? flyNow.vignette : 0.f,
                                                            rollComfort ? flyRollState.comfort() : 0.f);
                            if (rollComfort && !saved->flatScreen && validTrackedPose(rendered.headPose)) {
                                Vec3 ahead = rendered.headPose.orientation.rotate(Vec3{0, 0, -1});
                                ahead.y = 0;
                                if (length(ahead) > .1f) {
                                    ahead = normalized(ahead);
                                    // The rig rolls the world by flyRoll: the line is rolled back so it lies
                                    // along the game's horizon.
                                    const Quat back = Quat::around(ahead, -flyRollState.angle());
                                    const Vec3 side = back.rotate(Vec3{-ahead.z, 0, ahead.x});
                                    const Vec3 mid = viewer + ahead * 1.5f;
                                    soft::softBar(translucent, mid - side * .45f, mid + side * .45f, ahead, .003f,
                                                  Vec3{.85f, .95f, 1.f}, .55f, .004f);
                                }
                            }
                            if (flyShade > .01f && !saved->flatScreen && validTrackedPose(rendered.headPose)) {
                                const Quat q = rendered.headPose.orientation;
                                const float v = std::clamp(flyShade, 0.f, 1.f);
                                soft::softArc(translucent, viewer + q.rotate(Vec3{0, 0, -.3f}), q.rotate(Vec3{1, 0, 0}),
                                              q.rotate(Vec3{0, 1, 0}), .24f - .08f * v, .75f, 0, 6.2831853f,
                                              Vec3{0, 0, 0}, .85f * v, .10f, 48);
                            }
                            const float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() -
                                                                                      bloodStart).count();
                            bloodMean = bloodMean > 0 ? bloodMean * .95f + ms * .05f : ms;
                            bloodPeak = std::max(bloodPeak * .999f, ms);
                            publish(SpidyBloodData, [&](blood_splatter::Data& d) {
                                d.intensity = bloodIntensity.load();
                                d.fired = bloodPool.fired;
                                d.planB = bloodPool.planB;
                                d.confirmed = bloodPool.fired - bloodPool.planB;
                                d.meanMs = bloodMean;
                                d.peakMs = bloodPeak;
                            });
                        } else {
                            bloodLast = 0;
                            bloodPending = 0;
                        }
                        // The bursts where the latest blows landed: fixed in the world.
                        if (!saved->flatScreen)
                            for (const auto& burst : bursts) {
                                if (!burst.since)
                                    continue;
                                auto view = burst.view;
                                view.age = static_cast<float>(static_cast<double>(shown - burst.since) * 1e-9);
                                appendImpact(translucent, view, viewer, pixelAngle, saved->uiStyle);
                            }
                        // The called blow's arrow, over its thug.
                        if (!saved->flatScreen && cue.left > 0) {
                            auto view = cue;
                            view.age = static_cast<float>(static_cast<double>(shown - cueSince) * 1e-9);
                            if (view.age >= 0)
                                appendCue(translucent, view, viewer, pixelAngle, saved->uiStyle);
                        }
                        // A knockout's lettering, where he stood.
                        if (!saved->flatScreen)
                            for (const auto& mark : knockoutMarks) {
                                if (!mark.since)
                                    continue;
                                auto view = mark.view;
                                view.age = static_cast<float>(static_cast<double>(shown - mark.since) * 1e-9);
                                appendKnockout(translucent, view, viewer, pixelAngle, saved->uiStyle);
                            }
                        // Spider-Sense's marks: over each attacker's head, and for one out of
                        // view in the sightline as well, moved with the player as the eyes are.
                        if (!saved->flatScreen && validTrackedPose(rendered.headPose)) {
                            Pose eyes = rendered.headPose;
                            eyes.position += travel;
                            bool marked{}, sighted{};
                            const auto since = [&](int64_t at) {
                                return static_cast<float>(static_cast<double>(shown - at) * 1e-9);
                            };
                            for (auto& mark : senseMarks) {
                                if (!mark.attacker)
                                    continue;
                                const float age = since(mark.since);
                                const float broken = mark.brokenAt ? since(mark.brokenAt) : -1.f;
                                if (age < 0)
                                    continue;
                                SenseView view{mark.head, mark.answer, mark.left, mark.timed, age, broken};
                                appendSense(translucent, view, viewer, pixelAngle, saved->uiStyle);
                                mark.sight = senseSight(eyes, mark.head, mark.sight);
                                if (mark.sight.shown) {
                                    const SenseSightView sight{mark.answer, mark.sight, mark.left,
                                                               mark.timed,  age,        broken};
                                    appendSenseSight(translucent, sight, eyes, pixelAngle, {},
                                                     saved->uiStyle);
                                }
                                marked = true;
                                sighted |= mark.sight.shown;
                            }
                            senseFrames += marked;
                            senseSightFrames += sighted;
                        }
                        // The T-pose calibration's panel and controller rings, moved with
                        // the player to this image as the hands are. The tech style's
                        // panel is translucent, as the game's menus are.
                        if (!saved->flatScreen && saved->calibration.phase != body_calibration::Phase::idle) {
                            auto view = saved->calibration;
                            view.panel.position += travel;
                            for (auto& grip : view.grips)
                                grip.position += travel;
                            // Update 36: the other styles' panel is the textured title
                            // screen while the art texture can be used.
                            if (saved->uiStyle != UiStyle::tech && overlay.artReady()) {
                                const float clock = std::fmod(
                                    std::chrono::duration<float>(std::chrono::steady_clock::now() - artClock)
                                        .count(),
                                    1000.f);
                                calibration_panel::append(art, view, clock);
                                body_calibration::appendRings(vertices, view, viewer);
                            } else {
                                plainPanel = saved->uiStyle != UiStyle::tech;
                                auto& panel = saved->uiStyle == UiStyle::tech ? translucent : vertices;
                                body_calibration::appendView(panel, view, viewer, saved->uiStyle);
                            }
                        }
                        // Version 0.0.72 TEST BUILD NOTICE: over everything, moved with the player as the
                        // calibration panel is. Should it fail, only it goes off.
                        if (!saved->flatScreen && saved->notice.opacity > 0) {
                            const size_t from = translucent.size();
                            try {
                                auto notice = saved->notice;
                                notice.panel.position += travel;
                                test_notice::append(translucent, notice);
                            } catch (...) {
                                translucent.resize(from);
                            }
                        }
                        // Build 8 ROSTER page: its plate and text blended, its pictures in the textured models'
                        // pass (depth-tested, after). Nothing at all while it is closed.
                        if (!saved->flatScreen && saved->roster.opacity > 0) {
                            static thread_local roster_page::Cache rosterCache;
                            const size_t from = translucent.size(), modelsFrom = modelVertices.size();
                            try {
                                auto roster = saved->roster;
                                roster.panel.position += travel;
                                const mesh_model::Mesh* rosterMesh =
                                    overlay.modelReady() ? mesh_model::library().model(mesh_model::slotRoster) : nullptr;
                                roster_page::append(translucent, overlay.modelReady() ? &modelVertices : nullptr, roster,
                                                    rosterCache, rosterMesh);
                            } catch (...) {
                                translucent.resize(from);
                                modelVertices.resize(modelsFrom);
                            }
                        }
                        // The wrist hologram (Update 14) over its wrist, moved with the
                        // player to this image as the hands are, after the webs and
                        // the marks. Should it fail, only it goes off.
                        {
                            const unsigned hudHand = wristHudHand.load() ? 1u : 0u;
                            const float hudOpacity = wristToggle.opacity();
                            if (!saved->flatScreen && !ringHide && hudOpacity > 0 && wristHudOn.load() && !wristHudFault.load() &&
                                rendered.swing.hands[hudHand].tracked && validTrackedPose(rendered.grips[hudHand]) &&
                                validTrackedPose(rendered.headPose)) {
                                const size_t from = translucent.size();
                                try {
                                    // Update 38.1 RING LAYER NEWEST: the newest grip in this image's world.
                                    const auto newest = [&](unsigned h) {
                                        return ringNewest && latestGripsTracked[h] // 38.1 NEWEST only
                                                   ? combat38::ringWrist(rendered.grips[h], saved->worldFromTracking,
                                                                         latestToWorld, latestGrips[h], 0)
                                                   : rendered.grips[h];
                                    };
                                    auto grip = newest(hudHand);
                                    grip.position += travel;
                                    wrist_hud::View hud;
                                    hud.emitter = wrist_hud::emitter(grip, hudHand);
                                    hud.headUp = rendered.headPose.orientation.rotate({0, 1, 0});
                                    const Vec3 forward = rendered.headPose.orientation.rotate({0, 0, -1});
                                    hud.heading = std::atan2(forward.x, -forward.z);
                                    hud.opacity = hudOpacity;
                                    hud.scale = static_cast<float>(wristHudScalePercent.load()) / 100.f;
                                    // Update 17: its height, text size, theme and text colour.
                                    hud.heightCm = static_cast<float>(wristHudHeightCm.load());
                                    hud.textScale = static_cast<float>(wristHudTextPercent.load()) / 100.f;
                                    hud.theme = static_cast<int>(wristHudTheme.load());
                                    hud.textColor = static_cast<int>(wristHudTextColor.load());
                                    hud.seconds = static_cast<float>(std::fmod(static_cast<double>(shown) * 1e-9, 3600.));
                                    // Update 15: its page, and where the other hand points.
                                    hud.page = wristMenu.page();
                                    hud.scroll = wristMenu.scroll();
                                    if (!wristMenuFault.load()) {
                                        hud.settings = wristShown;
                                        hud.fps = wristFps.fps();
                                        hud.refreshHz = wristFps.refreshHz();
                                        hud.fpsTrail = wristFps.trail();
                                        hud.fpsHigh = wristSession.high();
                                        hud.fpsLow = wristSession.low();
                                        hud.fpsAvg = wristSession.average();
                                        hud.eyeGpuMs = native_gpu::eyePassesGpuMs();
                                        {   // Update 37.2: COLOR FX / FILTER and FOVEATED's buttons.
                                            hud.color = (colorFxOn.load() ? 1u : 0u) | (colorFilter.load() & 0xff) << 8 |
                                                        (colorRefused.load() ? 0x10000u : 0u) |
                                                        (colorOutlines.load() ? 0x20000u : 0u) | (colorBands.load() & 0xf) << 20 |
                                                        (native_gpu::eyeToonFault() ? 0x1000000u : 0u);
                                            const auto fs = native_gpu::foveaStatus();
                                            const uint32_t level = foveaLevelWant.load();
                                            hud.fovea = level | (fs.fault & 0xff) << 8 | (foveaDebugWant.load() ? 0x10000u : 0u) |
                                                        (fs.on ? 0x20000u : 0u);
                                            const auto& st = foveaPerf.at(level);
                                            hud.foveaFps[0] = st.frames ? st.high : 0.f;
                                            hud.foveaFps[1] = st.frames ? st.low : 0.f;
                                            hud.foveaFps[2] = st.avg();
                                            const float g = st.gpuAvg();
                                            hud.foveaGpuMs = std::isfinite(g) ? g : 0.f;
                                        }
                                        hud.hover = wristMenu.hover();
                                        // Update 38.5: the subtext being read and its scroll; its
                                        // overflow as drawn now goes back to the menu's next update.
                                        hud.helpFor = wristMenu.helpFor();
                                        hud.helpScroll = wristMenu.helpScroll();
                                        hud.held = wristMenu.held();
                                        hud.disabled = wristOff;
                                        hud.note = wristNote;
                                        hud.mirrorViewWhy = mirrorOffWhy; // MIRROR TEST
                                        hud.supersampleNow = static_cast<float>(native_gpu::supersample());
                                        {
                                            uint32_t st = 0, n = 0;
                                            native_address_finder::viewFields(st, n);
                                            hud.addressFinderStatus = st;
                                            if (st == 1)
                                                hud.addressFinderSeconds = n;
                                            else if (st == 2) {
                                                hud.addressFinderCandidates = n;
                                                hud.addressFinderAlmost = native_address_finder::almost();
                                                hud.addressFinderShadow = native_address_finder::shadowPasses();
                                            }
                                            hud.eyeLodStatus = SpidyEyeLodNow(); // Update 28
                                            hud.eyeLodReason = SpidyEyeLodReason(); // Update 28.1
                                            hud.shadowCamera = SpidyShadowCameraNow(); // Update 31
                                            hud.farShadows = SpidyFarShadowsNow(); // Update 35
                                            hud.eyeBloom = SpidyEyeBloomNow();     // Update 36
                                            hud.sharedLod = SpidySharedLodNow();   // Update 36
                                            hud.mirror = (mirrorOn.load() ? 1u : 0u) | (mirrorOn.load() && mirrorHalfRate.load() ? 2u : 0u) |
                                                         (!mirrorOn.load() && mirrorAutoOffAtMs.load() ? 4u : 0u) |
                                                         std::min<uint32_t>(mirrorMicros.load(), 0xffffffu) << 8;
                                            hud.blood = bloodIntensity.load(); // Update 37
                                            hud.bloodVisor = bloodVisor.load(); // Update 38.5
                                            hud.invincible = invincibleOn.load(); // Update 38.5 EXTRAS
                                            hud.invincibleNa = invincibleNa.load() != 0;
                                            hud.flyMode = flyModeNow.load();
                                            hud.flyVignette = flyVignetteOn.load();
                                            hud.flyRoll = flyRollNow.load();
                                            hud.flyAutoLevel = flyAutoLevelNow.load();
                                            hud.symbiote = symbioteOn.load();
                                            hud.padMode = padLayer.latched();
                                            hud.padByPrompt = padLayer.byPrompt();
                                            hud.promptHandling = promptHandlingNow.load();
                                            hud.gestureMoves = gestureMovesNow.load();
                                            hud.trainMove = training_guide::moveName(trainMovePicked); // Version 0.0.2
                                            hud.trainMine = trainMine;
                                            hud.remap = remapFault ? nullptr : &remapView; // Update 38.5 REMAP
                                            hud.monitorArmed = monitorConfirm.armed(static_cast<float>(extrasClock));
                                            hud.mirrorAnchor = mirrorAnchor.load(); // Update 37.2
                                            hud.npcFade = npc_fade::now();          // Update 37.2
                                            for (uint32_t o = 0; o < combat38::options; ++o) // Update 38
                                                hud.combat38[o] = c38(o);
                                            hud.slotTest = slotTestPacked.load();   // Update 37.2
                                            hud.webBall = webBallPacked.load() | (SpidyEyeBloomNow() & 0xff) << 16;
                                            {   // Update 37: the CONTROLS page, as the live settings stand now
                                                controls_guide::Live g;
                                                const auto cc = combatConfig();
                                                g.combat = cc.on;
                                                g.grabberSlot = cc.grabberSlot != combat_input::slotOff;
                                                g.sonicSlot = cc.sonicSlot != combat_input::slotOff;
                                                g.upshotSlot = cc.upshotSlot != combat_input::slotOff;
                                                g.ricochetSlot = cc.ricochetSlot != combat_input::slotOff;
                                                g.punch = cc.punch;
                                                g.steer = cc.steer;
                                                g.heal = healOn.load();
                                                g.climb = climbOn.load();
                                                g.gameCrawl = gameWallCrawl.load();
                                                g.mirror = mirrorOn.load();
                                                g.blood = bloodIntensity.load() != 0;
                                                g.worldLog = world_trace::isOn();
                                                g.pad = padInstalled;
                                                controls_guide::masks(g, hud.guideOff, hud.guideNa);
                                            }
                                            {
                                                const auto& w = SpidyWorldTraceData;
                                                uint64_t logged{};
                                                for (unsigned e = 1; e < 7; ++e)
                                                    logged += w.counts[e];
                                                hud.worldLog = (world_trace::isOn() ? 1u : 0u) | (w.status == 2 ? 2u : 0u) |
                                                               static_cast<uint32_t>(std::min<uint64_t>(logged, 0xffffff)) << 8;
                                            }
                                            hud.shadowDistance = native_shadow_distance::now(); // Update 33
                                            hud.eyeAa = eyeAaNow();
                                            hud.engineTaa = SpidyEngineTaaNow(); // Update 34
                                            hud.taaJitter = SpidyEngineTaaJitterNow(); // Update 34.2
                                            hud.eyeAaMs = native_gpu::eyeHistoryMs();
                                            hud.shadowDistanceGame = native_shadow_distance::gameReach();
                                            hud.shadowDistanceLive = native_shadow_distance::liveReach();
                                        }
                                        if (hud.page == wrist_hud::musicPage)
                                            hud.music = wristMusic; // Update 19
                                        const unsigned pointHand = 1 - hudHand;
                                        if (wristPointer.hit && (wrist_hud::onPanel(wristMenu.page(), wristPointer.u, wristPointer.v) ||
                                                                 wristMenu.held() != wrist_hud::Control::none) &&
                                            validTrackedPose(rendered.grips[pointHand])) {
                                            // Drawn from this image's hand, as the panel is.
                                            auto pointGrip = newest(pointHand);
                                            pointGrip.position += travel;
                                            const auto f = wrist_hud::place(hud, viewer);
                                            const auto now = wrist_hud::point(f, hud.scale, pointGrip, pointHand);
                                            if (now.hit) {
                                                hud.pointing = true;
                                                hud.tip = now.tip;
                                                hud.pointU = now.u;
                                                hud.pointV = now.v;
                                            }
                                        }
                                    } else {
                                        hud.page = 0;
                                    }
                                    {   // Update 37: HOLO REUSE (perf37.hpp) - rebuilt at most every other
                                        // frame while nothing on it changed and it sits still.
                                        const auto holoStart = std::chrono::steady_clock::now();
                                        const auto f = wrist_hud::place(hud, viewer);
                                        perf37::HoloKey key;
                                        key.page = static_cast<int>(hud.page);
                                        key.scroll = static_cast<float>(hud.scroll);
                                        key.hover = static_cast<uint32_t>(hud.hover);
                                        key.help = hud.helpFor == wrist_hud::Control::none
                                                       ? 0.f
                                                       : hud.helpScroll + 64.f * static_cast<float>(hud.helpFor);
                                        if (hud.helpFor != wrist_hud::Control::none) {
                                            const int over = wrist_hud::helpOverflow(hud, hud.page, hud.helpFor);
                                            wristHelpOverflow.store(static_cast<uint32_t>(hud.helpFor) << 8 |
                                                                    static_cast<uint32_t>(std::clamp(over, 0, 254) + 1));
                                        }
                                        key.held = static_cast<uint32_t>(hud.held);
                                        key.theme = static_cast<uint32_t>(hud.theme) | static_cast<uint32_t>(hud.textColor) << 8;
                                        key.extra = static_cast<uint32_t>(hud.opacity * 255.f) |
                                                    static_cast<uint32_t>(hud.scale * 100.f) << 8 |
                                                    static_cast<uint32_t>(hud.textScale * 100.f) << 20;
                                        key.pointing = hud.pointing;
                                        const bool reuse = holoReuseOn.load() && holoReuse.mayReuse(key, f);
                                        if (reuse) {
                                            holoReuse.replay(translucent, f);
                                            ++holoReused;
                                        } else {
                                            wrist_hud::appendWristHud(translucent, hud, viewer, pixelAngle);
                                            ++holoBuilt;
                                            if (holoReuseOn.load())
                                                holoReuse.store(translucent, from, key, f);
                                            else if (holoReuse.kept())
                                                holoReuse.reset(); // OFF keeps nothing
                                        }
                                        const double ms = std::chrono::duration<double, std::milli>(
                                                              std::chrono::steady_clock::now() - holoStart).count();
                                        (reuse ? holoReplay : holoBuild).add(ms);
                                        holoShown.add(ms);
                                    }
                                    if (!wrist_hud::sane(translucent, from, hud.emitter, 1.5f))
                                        throw std::runtime_error("unfit vertices");
                                } catch (...) {
                                    translucent.resize(from);
                                    holoReuse.reset();
                                    wristHudFault = true;
                                    message(3, 0, "Wrist hologram failed and is off (save hud = 0, then "
                                                  "hud = 1, in live.txt to try it again); everything "
                                                  "else carries on");
                                }
                            }
                        }
                        const auto extrasStart = std::chrono::steady_clock::now(); // Update 37 perf
                        hudPart.add(std::chrono::duration<double, std::milli>(extrasStart - overlayStart).count());
                        // The gadget wheel in front of its hand, moved with the
                        // player to this image as the hands are: printed, so
                        // laid down in order over all the rest.
                        if (!saved->flatScreen && saved->gadgetWheel.open) {
                            auto view = saved->gadgetWheel;
                            view.frame.centre += travel;
                            appendGadgetWheel(translucent, view, viewer, pixelAngle, saved->uiStyle);
                        }
                        // The phone where it is now - in the hand, where the hand is
                        // now - through this image's own way into its world, and
                        // moved with the player to this image as the wheel is.
                        if (!saved->flatScreen && saved->phone.open) {
                            auto view = phoneShown;
                            const Pose& toWorld = saved->phone.toWorld;
                            const auto& f = phone.place();
                            view.open = true;
                            view.age = phone.age();
                            const Quat turn = toWorld.orientation;
                            view.frame = {compose(toWorld, Pose{f.centre, {}}).position + travel,
                                          turn.rotate(f.right), turn.rotate(f.up), turn.rotate(f.out)};
                            appendPhone(vertices, view);
                        }
                        std::array<D3D12Renderer::ViewTarget, 2> overlayViews;
                        // Slow motion recolours the image under the overlay, about each eye's lens.
                        D3D12Renderer::Grade grade;
                        grade.amount = saved->slow.blend;
                        grade.ripple = saved->slow.ripple;
                        grade.rippleStrength = saved->slow.entering ? 1.f : .7f;
                        grade.entering = saved->slow.entering;
                        for (unsigned eye = 0; eye < 2; ++eye) {
                            const auto& f = rendered.fovs[made(eye)];
                            grade.tangents[eye] = {std::tan(f.left), std::tan(f.right), std::tan(f.up),
                                                   std::tan(f.down)};
                        }
                        for (unsigned eye = 0; eye < 2; ++eye) {
                            const auto& target = targets[eye];
                            const auto& f = rendered.fovs[made(eye)];
                            auto pose = rendered.eyes[made(eye)];
                            pose[12] += travel.x;
                            pose[13] += travel.y;
                            pose[14] += travel.z;
                            const auto vp = multiply(projection(f.left, f.right, f.down, f.up, .03f, 200.f),
                                                     native_view::view(pose));
                            overlayViews[eye] = {target.texture, target.format, target.width, target.height,
                                                 vp};
                        }
                        const auto drawStart = std::chrono::steady_clock::now(); // Update 37 perf
                        extrasPart.add(std::chrono::duration<double, std::milli>(drawStart - extrasStart).count());
                        if (!saved->flatScreen && !overlayFault) {
                            try {
                                overlay.renderViews(overlayViews, vertices, true, false, translucent, &grade, art, modelVertices);
                            } catch (const std::exception& e) {
                                overlayFault = true;
                                // (The message holds 255 characters: the reason first.)
                                const auto text = std::string("VR continues; Spidy's drawings are off this session: ") +
                                                  e.what();
                                message(3, 0, text.c_str());
                                OutputDebugStringA(text.c_str());
                            }
                            if (!overlayArtNoted && (plainPanel || !art.empty()) && !overlay.artReady()) {
                                overlayArtNoted = true;
                                const auto text = "VR continues, the calibration in its plain panel: " +
                                                  (overlay.artFault().empty() ? artMissing : overlay.artFault());
                                message(3, 0, text.c_str());
                                OutputDebugStringA(text.c_str());
                            }
                            if (!overlayShaderNoted && !overlay.shaderFault().empty()) {
                                overlayShaderNoted = true;
                                const auto text = "VR continues, drawings in plain colours: " + overlay.shaderFault();
                                message(3, 0, text.c_str());
                                OutputDebugStringA(text.c_str());
                            }
                        }
                        overlayMs = std::chrono::duration<double, std::milli>(
                                        std::chrono::steady_clock::now() - overlayStart)
                                        .count();
                        {   // Update 37 performance pass: the record (PRF7, perf37.hpp), read by the runner.
                            drawPart.add(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                                   drawStart).count());
                            overlayPart.add(overlayMs);
                            if (++perfFrames % 45 == 0)
                                publish(SpidyPerfData, [&](perf37::Data& d) {
                                    d.holoReuse = holoReuseOn.load() ? 1u : 0u;
                                    d.holoBuilt = holoBuilt;
                                    d.holoReused = holoReused;
                                    d.holoBuildMs = static_cast<float>(holoBuild.mean());
                                    d.holoBuildPeakMs = static_cast<float>(holoBuild.peak);
                                    d.holoReplayMs = static_cast<float>(holoReplay.mean());
                                    d.holoShownMs = static_cast<float>(holoShown.mean());
                                    for (unsigned i = 0; i < perf37::sections; ++i) {
                                        d.sectionMean[i] = static_cast<float>(perfSections.metrics[i].mean());
                                        d.sectionPeak[i] = static_cast<float>(perfSections.metrics[i].peak);
                                    }
                                    d.spikeMs = static_cast<float>(perfSections.spike.ms);
                                    d.spikeSectionMs = static_cast<float>(perfSections.spike.sectionMs);
                                    d.spikeSeconds = static_cast<float>(perfSections.spike.seconds);
                                    d.spikeSection = perfSections.spike.section;
                                    d.spikes = perfSections.spike.count;
                                    d.frames = perfFrames;
                                    d.hudMean = static_cast<float>(hudPart.mean());
                                    d.hudPeak = static_cast<float>(hudPart.peak);
                                    d.extrasMean = static_cast<float>(extrasPart.mean());
                                    d.extrasPeak = static_cast<float>(extrasPart.peak);
                                    d.drawMean = static_cast<float>(drawPart.mean());
                                    d.drawPeak = static_cast<float>(drawPart.peak);
                                    d.overlayMean = static_cast<float>(overlayPart.mean());
                                    d.overlayPeak = static_cast<float>(overlayPart.peak);
                                    d.bloodMean = bloodMean;
                                    d.mirrorMean = static_cast<float>(mirrorMicros.load()) / 1000.f;
                                    d.worldScanMean = SpidyWorldTraceData.meanScanMs;
                                });
                        }
                        // The game's window mirrors the left eye as it is now, the
                        // drawings above on it; copied only when the window took the last.
                        native_gpu::keepShownEye(targets[0].texture);
                        if (webGap >= 0 && imageGeneration != gapGeneration) {
                            native_eyes::reportWebGap(webGap);
                            gapGeneration = imageGeneration;
                        }
                        // Session report: publish a finished copy of the left
                        // eye, then queue the next when one is due.
                        D3D12Renderer::Captured pixels;
                        if (snapshotTicket && overlay.captured(snapshotTicket, pixels)) {
                            publishSnapshot(pendingSnapshot, &pixels);
                            snapshotTicket = 0;
                            // Update 38.3 SUIT COLOURS: the suit as the game drew it in this copy.
                            if (suitPxCount) {
                                suitColours.beginCopy();
                                for (uint32_t k = 0; k < suitPxCount; ++k) {
                                    Vec3 colour;
                                    if (vr_mirror::samplePatch(pixels.pixels, pixels.width, pixels.height,
                                                               pixels.rowPitch, suitPx[k][0], suitPx[k][1], colour))
                                        suitColours.add(colour);
                                }
                                // Update 38.5: after a hero switch the last suit stays until the new one is learned.
                                const vr_mirror::SuitPalette learnedNow = suitColours.palette();
                                if (learnedNow.learned || !suitPalette.learned || suitColours.forgets() == 0)
                                    suitPalette = learnedNow;
                                suitPxCount = 0;
                            }
                        }
                        const float speed = swingState.status ? length(swingState.velocity) : 0;
                        if (!snapshotTicket && snapshots.due(GetTickCount64(), speed, ropeHands != 0) &&
                            (snapshotTicket = overlay.capture(targets[0].texture)) != 0) {
                            pendingSnapshot = {};
                            pendingSnapshot.flags = ropeHands;
                            pendingSnapshot.generation = imageGeneration;
                            pendingSnapshot.serial = imageSerial;
                            pendingSnapshot.flatScreen = saved->flatScreen;
                            pendingSnapshot.speed = speed;
                            pendingSnapshot.webGap = webGap;
                            const auto& eye = overlayViews[0];
                            for (unsigned hand = 0; hand < 2; ++hand) {
                                if (shooterHands & (1u << hand))
                                    eyePixel(eye.viewProjection, shooters[hand], eye.width, eye.height,
                                             pendingSnapshot.wrist[hand][0], pendingSnapshot.wrist[hand][1]);
                                if (ropeHands & (1u << hand))
                                    eyePixel(eye.viewProjection, ropeStarts[hand], eye.width, eye.height,
                                             pendingSnapshot.ropeStart[hand][0],
                                             pendingSnapshot.ropeStart[hand][1]);
                            }
                            // Update 38.3 SUIT COLOURS: where the hero's limbs are in this copy (the
                            // mirror look SUIT, the body drawn and freshly solved, the mirror's own
                            // figure well away from the gaze).
                            suitPxCount = 0;
                            native_body::Suit suit;
                            if (!saved->flatScreen && c38(combat38::mirrorLook) == 0 && !mirrorInSight &&
                                native_body::drawn() && native_body::suit(suit) &&
                                GetTickCount64() - suit.solvedAt < 250) {
                                Vec3 joints[native_body::suitJoints], points[vr_mirror::suitSamplePoints];
                                const Vec3 feet = rendered.anchor + travel;
                                for (uint32_t j = 0; j < native_body::suitJoints; ++j)
                                    joints[j] = feet + suit.joints[j];
                                vr_mirror::samplePoints(joints, points);
                                for (const Vec3& point : points)
                                    if (eyePixel(eye.viewProjection, point, eye.width, eye.height,
                                                 suitPx[suitPxCount][0], suitPx[suitPxCount][1]))
                                        ++suitPxCount;
                            }
                        }
                    }
                    std::lock_guard lock(telemetry);
                    auto& d = SpidyXrData;
                    InterlockedIncrement64(&d.sequence);
                    d.dropped += !copied;
                    d.serial = imageSerial;
                    d.generation = imageGeneration;
                    d.markers = markersDrawn;
                    InterlockedIncrement64(&d.sequence);
                    return copied;
                });
            // No headset frame for a while: slow motion ends on its own, in real time.
            if (const auto now = GetTickCount64(); now - slowSteppedMs >= 200)
                stepSlowMotion(static_cast<float>(now - slowSteppedMs) / 1000, false, false);
            const auto& timing = runtime.lastFrameTiming();
            if (submittedFrames >= 30 && timing.period > 0) {
                const double stages[] = {timing.total,   timing.wait,  timing.tracking, timing.prepare,
                                         timing.acquire, copyMs,       overlayMs,       timing.release,
                                         timing.end,     timing.period};
                std::lock_guard lock(telemetry);
                InterlockedIncrement64(&SpidyXrTimingData.sequence);
                for (unsigned i = 0; i < 10; ++i)
                    SpidyXrTimingData.stages[i].add(stages[i]);
                native_gpu::copyTiming(SpidyXrTimingData.stages[10], SpidyXrTimingData.stages[11]);
                native_gpu::eyeAATiming(SpidyXrTimingData.stages[12], SpidyXrTimingData.eyeAA,
                                        SpidyXrTimingData.eyeAAFault, SpidyXrTimingData.eyeAAPairs);
                native_gpu::eyeSharpenTiming(SpidyXrTimingData.eyeSharpen, SpidyXrTimingData.sharpen,
                                             SpidyXrTimingData.sharpenFault, SpidyXrTimingData.sharpenPairs);
                InterlockedIncrement64(&SpidyXrTimingData.sequence);
            }
            // Count only a successful xrEndFrame containing a projection layer.
            // A GPU copy alone does not confirm that OpenXR accepted the frame.
            const bool submitted = runtime.lastFrameSubmitted();
            if (submitted) {
                ++submittedFrames;
                lastPresentedMs = GetTickCount64();
                // Update 32.1: a graphics setting rebuilt the eye images.
                if (const auto swaps = native_gpu::eyeSwaps(); gpuStarted && swaps != eyeSwapsSeen) {
                    eyeSwapsSeen = swaps;
                    message(3, 0, eye_swap::swappedMessage);
                }
                if (gpuStarted && native_gpu::eyesLost() && !eyesLostSeen) {
                    eyesLostSeen = true;
                    message(3, 0, eye_swap::lostMessage);
                }
                if (!presented || drawn != shownKind) {
                    presented = true;
                    shownKind = drawn;
                    // The game played on its screen shows it there, flat or in 3D.
                    const bool screenPlay =
                        flatScreen && (drawn == Presentation::gameScreen || drawn == Presentation::stereoScreen);
                    message(3, 0,
                            native_gpu::eyesLost() ? eye_swap::lostMessage
                            : screenPlay
                                ? "The game on its screen: the controllers work as an Xbox controller; hold the "
                                  "menu button for its Back (the map), hold the left stick's click and push the "
                                  "right stick for its D-pad - pause, SWITCH TO VR goes back to VR"
                            : drawn == Presentation::gameScreen
                                ? "Game screen (menus, loading, cutscenes): the controllers work as an Xbox "
                                  "controller, VR settings are in Settings > SPIDY VR - VR resumes with gameplay"
                            : drawn == Presentation::stereoScreen
                                ? "Game screen in 3D (a cutscene or another scene the game plays): the "
                                  "controllers work as an Xbox controller - Settings > SPIDY VR > GAME SCREEN 3D "
                                  "sets its depth"
                            : drawn == Presentation::flat
                                ? "Flat screen - pause, SWITCH TO VR goes back to VR"
                                // 255 characters, all the message holds.
                                : "VR active - A jumps (hold: Charge Jump), B dodges, Y interacts, X: game "
                                  "menu (hold: gadget wheel), menu or both sticks held: pause, left stick "
                                  "click: slow time, trigger at temple: scan; right stick click: LB + RB, "
                                  "down: heal, up: zip (hold to aim, let go)");
                }
            }
            {
                std::lock_guard lock(telemetry);
                auto& d = SpidyXrData;
                InterlockedIncrement64(&d.sequence);
                d.presentation = static_cast<uint32_t>(submitted ? drawn : Presentation::none);
                if (submitted) {
                    ++d.submitted;
                    // Game-screen frames are not eye images: neither new nor reused ones.
                    if (drawn == Presentation::gameScreen || drawn == Presentation::stereoScreen) {
                        ++d.screenSubmitted;
                        d.stereoSubmitted += drawn == Presentation::stereoScreen;
                    } else if (d.generation != previousPresentedGeneration) {
                        ++d.uniqueSubmitted;
                        lastNewImageMs = lastPresentedMs;
                        previousPresentedGeneration = d.generation;
                    } else {
                        ++d.reusedSubmitted;
                    }
                }
                InterlockedIncrement64(&d.sequence);
            }
            if (!active) {
                runtimeEnded = true;
                break;
            }
        }
        message(6, 0, "Stopping VR and restoring game hooks");
    } catch (const std::exception& e) {
        error = 1;
        failure = e.what();
    }
    // Update 24.1: a part that does not stop cleanly adds to the reason VR
    // ended; it no longer hides the first one.
    const auto stopFailure = [&](const char* text) {
        failure = failure.empty() ? std::string(text) : failure + "; then: " + text;
    };
    players.stop();
    npc_fade::stop(); // the game's bytes back
    world_trace::stop();
    wrist_muzzle::stop();
    if (padInstalled)
        game_pad::uninstall();
    game_menu::uninstall();
    // The game's own time back, and no blow still sounding. The ticket's
    // guard stays for as long as the game runs: it is what keeps a quit to
    // the main menu after VR from closing the game.
    game_time::uninstall();
    PlaySoundW(nullptr, nullptr, 0);
    bridge::Control release;
    release.serial = ++serial;
    release.leaseMs = 0;
    if (submitInput)
        submitInput(&release);
    if (swingStarted) {
        const auto code = stopSwing(nullptr);
        if (code) {
            error = code;
            stopFailure("Native swinging did not stop cleanly");
        }
    }
    if (websStarted) {
        const auto code = native_webs::stop();
        if (code) {
            error = code;
            stopFailure("Game web lines did not stop cleanly");
        }
    }
    native_game_body::end();
    if (bodyStarted) {
        const auto code = native_body::stop();
        if (code) {
            error = code;
            stopFailure("The player's body did not stop cleanly");
        }
    }
    if (nativeStarted) {
        const auto code = SpidyStop(nullptr);
        if (code) {
            error = code;
            stopFailure("Native views did not retire cleanly");
        }
    }
    if (gpuStarted) {
        const auto code = native_gpu::stop();
        if (code) {
            error = code;
            stopFailure("Native GPU bridge did not stop cleanly");
        }
    }
    if (raysStarted) {
        const auto code = stopRays(nullptr);
        if (code) {
            error = code;
            stopFailure("Controller world rays did not stop cleanly");
        }
    }
    if (stopBridge) {
        const auto code = stopBridge(nullptr);
        if (code) {
            error = code;
            stopFailure("Input bridge did not stop cleanly");
        }
    }
    message(error ? 5 : 4, error,
            error          ? failure.c_str()
            : runtimeEnded ? "VR session finished: the VR runtime closed it (headset, Virtual Desktop or SteamVR)"
                           : "VR session finished");
    return error;
}
} // namespace
// Update 34.1: every option bit this build knows (the highest: 0x800000, -EngineTaa).
// tests/launcher_options_tests.py checks run_game_vr.py sends none past it.
constexpr uint32_t knownOptions = 0xffffff;
extern "C" __declspec(dllexport) DWORD WINAPI SpidyXrStart(void* input) {
    std::lock_guard life(lifecycle);
    if (worker)
        return 1000; // one bounded XR session per process during validation
    if (!read(reinterpret_cast<uintptr_t>(input), &config, sizeof(config)) || config.magic != 0x53585243 ||
        config.version != 24 || config.bytes != sizeof(config) || config.pid != GetCurrentProcessId() ||
        config.runtime[std::size(config.runtime) - 1] ||
        config.base != reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr)) ||
        !GetModuleHandleW(L"Spider-Man2.exe") || !config.queue || !config.bridgeModule || !config.rayModule ||
        !config.motionModule || config.snapTurn > 90 || config.haptics > 100 ||
        !validEyeUpscale(config.upscale) || !validMonitorRate(config.monitorRate) ||
        config.screenSize > 2 || config.screenDepth > 3 || config.bodyHaptics > 100 || config.smoothTurn > 360 ||
        config.hud > 3 || config.flipSpeed < 90 ||
        config.flipSpeed > 480 || !std::isfinite(config.swingSpeed) || config.swingSpeed < 1 ||
        config.swingSpeed > 65 || (config.durationMs && config.durationMs < 2000) || config.durationMs > 25000 ||
        (config.eyeSize && !validEyeSize(config.eyeSize)) || !validRenderScale(config.renderScale) ||
        (config.eyeSize && config.renderScale != 100) || config.weight < 40 || config.weight > 300)
        return 1001;
    // Update 34.1: an option this build does not know is skipped, never a reason to start no VR.
    if (const uint32_t unknown = config.options & ~knownOptions) {
        char line[128];
        std::snprintf(line, sizeof line, "VR start: options 0x%x not known to this build, skipped\n", unknown);
        OutputDebugStringA(line);
        config.options &= knownOptions;
    }
    // A calibration is both measurements or neither.
    if ((config.eyeHeightMm == 0) != (config.armLengthMm == 0) ||
        (config.eyeHeightMm &&
         (config.eyeHeightMm < body_calibration::minEyeHeightMm || config.eyeHeightMm > body_calibration::maxEyeHeightMm ||
          config.armLengthMm < body_calibration::minArmLengthMm || config.armLengthMm > body_calibration::maxArmLengthMm)))
        return 1001;
    auto module = reinterpret_cast<HMODULE>(config.bridgeModule);
    submitInput = reinterpret_cast<BridgeCall>(GetProcAddress(module, "SpidySubmit"));
    sampleBridge = reinterpret_cast<BridgeCall>(GetProcAddress(module, "SpidyBridgeSample"));
    stopBridge = reinterpret_cast<BridgeCall>(GetProcAddress(module, "SpidyStop"));
    retargetBridge = reinterpret_cast<BridgeCall>(GetProcAddress(module, "SpidyRetarget"));
    auto rays = reinterpret_cast<HMODULE>(config.rayModule);
    startRays = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyRayStart"));
    submitRays = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyRaySubmit"));
    stopRays = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyRayStop"));
    startSwing = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingStart"));
    submitSwing = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingSubmit"));
    sampleSwing = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingSample"));
    sampleBends = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingBends"));
    sampleClimb = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyClimbSample"));
    rayResults = reinterpret_cast<const native_rays::Data*>(GetProcAddress(rays, "SpidyRayData"));
    stopSwing = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingStop"));
    retargetSwing = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingRetarget"));
    // Optional: an older ray module has no web grab, nor punching, nor aim previews.
    sampleGrab = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyGrabSample"));
    sampleAim = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyAimSample"));
    startPunch = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyPunchStart"));
    samplePunch = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyPunchSample"));
    // Optional too: without them the VR settings' grab, speed and punch
    // switches take effect only at the next session.
    stopPunch = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyPunchStop"));
    // Optional: an older ray module has no web strike, and a punch only its short pulse.
    sampleStrike = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyStrikeSample"));
    // Optional (Update 38): an older ray module has no SPIDYVR MOVE lunge (LUNGE stays GAME BUTTON).
    sampleLunge = reinterpret_cast<BridgeCall>(reinterpret_cast<void*>(GetProcAddress(rays, "SpidyLungeSample")));
    sampleAirYield = reinterpret_cast<BridgeCall>(reinterpret_cast<void*>(GetProcAddress(rays, "SpidyAirYieldSample")));
    sampleFly = reinterpret_cast<BridgeCall>(reinterpret_cast<void*>(GetProcAddress(rays, "SpidyFlySample")));
    sampleCape = reinterpret_cast<BridgeCall>(reinterpret_cast<void*>(GetProcAddress(rays, "SpidyCapeSample")));
    sampleSense = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySenseSample"));
    swingSettings = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidySwingSettings"));
    // Optional: an older ray module has no web shooter.
    startShooter = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyShooterStart"));
    sampleShooter = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyShooterSample"));
    sampleWarp = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyWarpSample"));
    sampleOffers = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyOffersSample"));
    useOffer = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyOffersUse"));
    aimOffer = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyOffersAim"));
    stopShooter = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyShooterStop"));
    wheelShooter = reinterpret_cast<BridgeCall>(GetProcAddress(rays, "SpidyShooterGadgets"));
    if (!submitInput || !sampleBridge || !stopBridge || !retargetBridge || !startRays || !submitRays ||
        !stopRays || !startSwing || !submitSwing || !sampleSwing || !stopSwing || !retargetSwing)
        return 1002;
    stopRequested = false;
    // The worker attaches each player it finds; none yet.
    native_appearance::setPlayerRecord(0);
    keepAliveDeadline = GetTickCount64() + 10000;
    // Version 0.0.6: the start banner names the version and its maker.
#if defined(SPIDY_TEST_BUILD) && SPIDY_TEST_BUILD
    // Build 5: the first line names the code that runs (build 4's console said only 0.0.70).
    message(1, 0, "HERO VR 0.0.72 TEST " SPIDY_TEST_NUMBER " " SPIDY_TEST_COMMIT " - Made by Agent Mulder: starting OpenXR");
#else
    message(1, 0, "SM2 VR " SPIDY_PUBLIC_VERSION " - Made by Agent Mulder: starting OpenXR");
#endif
    // Version 0.0.2 hotfix: the VR thread reserves its own 8 MB of stack (address space only;
    // pages are committed as used) instead of the game's default (1 MB), so its deep frame
    // has room whatever the game's own default is.
    worker = CreateThread(nullptr, xrThreadStackReserve, run, nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    return worker ? 0 : 1003;
}
// The webs' width across (live.txt web_width, cm), in tenths of a millimetre:
// 40-400 (0.4-4 cm); refused (3964) otherwise. Takes effect with the next image.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWebWidth(void* tenthsMm) {
    const auto width = reinterpret_cast<uintptr_t>(tenthsMm);
    if (width < static_cast<uintptr_t>(WebLook::minRadius * 2e4f + .5f) ||
        width > static_cast<uintptr_t>(WebLook::maxRadius * 2e4f + .5f))
        return 3964;
    webRadiusTenthsMm = static_cast<uint32_t>((width + 1) / 2);
    return 0;
}
// Update 23: where webs are drawn leaving the hand (live.txt web_start): 0 the
// drawn wrist's web shooter, 1 Update 20's spot under the palm; refused
// (3990) otherwise. Live, only the picture.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWebStart(void* spot) {
    const auto value = reinterpret_cast<uintptr_t>(spot);
    if (value >= webStarts)
        return 3990;
    webStartSpot = static_cast<uint32_t>(value);
    return 0;
}
// The shot's flight (Update 15, live.txt web_travel): 1 the tip flies to the
// anchor, 0 the web is drawn whole at once; refused (3968) otherwise.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWebTravel(void* on) {
    const auto value = reinterpret_cast<uintptr_t>(on);
    if (value > 1)
        return 3968;
    webTravelOn = value != 0;
    return 0;
}
// The shot tip's speed (live.txt web_speed), m/s, 20-2000; refused (3969)
// otherwise. The flight still lasts 0.03-0.11 s.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWebSpeed(void* metresPerSecond) {
    const auto value = reinterpret_cast<uintptr_t>(metresPerSecond);
    if (value < static_cast<uintptr_t>(WebTravel::minSpeed) || value > static_cast<uintptr_t>(WebTravel::maxSpeed))
        return 3969;
    webSpeedMs = static_cast<uint32_t>(value);
    return 0;
}
// Update 37 live settings (tools/live_settings.py). Each takes a whole number, refused with
// its own code outside the range given; all take effect at once.
namespace {
template <class T, class V> DWORD setLive(void* in, uintptr_t low, uintptr_t high, DWORD refused, std::atomic<T>& to, V convert) {
    const auto value = reinterpret_cast<uintptr_t>(in);
    if (value < low || value > high)
        return refused;
    to = convert(value);
    return 0;
}
auto asBool = [](uintptr_t v) { return v != 0; };
auto asU32 = [](uintptr_t v) { return static_cast<uint32_t>(v); };
} // namespace
// Strand physics (web_physics 0/1, web_wiggle and web_bend percent 0-300).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWebPhysics(void* on) { return setLive(on, 0, 1, 5101, webPhysicsOn, asBool); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWebWiggle(void* percent) {
    return setLive(percent, 0, static_cast<uintptr_t>(WebTravel::maxWiggle), 5102, webWiggle, asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWebBend(void* percent) {
    return setLive(percent, 0, static_cast<uintptr_t>(WebTravel::maxBend), 5103, webBend, asU32);
}
// Combat by gesture (combat_input.hpp).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyCombat(void* on) { return setLive(on, 0, 1, 5110, combatOn, asBool); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyCombatTapMs(void* ms) {
    return setLive(ms, combat_input::Config::minTapMs, combat_input::Config::maxTapMs, 5111, combatTapMs, asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyCombatGestureMs(void* ms) {
    return setLive(ms, combat_input::Config::minGestureMs, combat_input::Config::maxGestureMs, 5112, combatGestureMs, asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGrabberSlot(void* slot) { return setLive(slot, 0, 4, 5113, grabberSlot, asU32); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidySonicSlot(void* slot) { return setLive(slot, 0, 4, 5114, sonicSlot, asU32); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyUpshotSlot(void* slot) { return setLive(slot, 0, 4, 5115, upshotSlot, asU32); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyRicochetSlot(void* slot) { return setLive(slot, 0, 4, 5116, ricochetSlot, asU32); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyYankSpeed(void* cms) { return setLive(cms, 50, 500, 5117, yankCms, asU32); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyUpshotSpeed(void* cms) { return setLive(cms, 50, 500, 5118, upshotCms, asU32); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyUpshotLow(void* degrees) { return setLive(degrees, 0, 60, 5119, upshotLowDegrees, asU32); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFlickSpeed(void* cms) { return setLive(cms, 50, 500, 5120, flickCms, asU32); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidySonicGap(void* cm) { return setLive(cm, 5, 60, 5121, sonicGapCm, asU32); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidySonicReach(void* cm) { return setLive(cm, 5, 70, 5122, sonicReachCm, asU32); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPunch(void* on) { return setLive(on, 0, 1, 5123, punchOn, asBool); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPunchSpeed(void* cms) { return setLive(cms, 100, 800, 5124, punchCms, asU32); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyAimSteer(void* on) { return setLive(on, 0, 1, 5125, aimSteerOn, asBool); }
// Update 37.2: ZIP STRIKE, WEB YANK and the yank's slot (0 off .. 6 Y, combat_input::Slot).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyZipStrike(void* on) { return setLive(on, 0, 1, 5190, zipStrikeOn, asBool); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWebYank(void* on) { return setLive(on, 0, 1, 5191, webYankOn, asBool); }
// Update 37.2: EYE AA AT START (live.txt eye_temporal, 1 by default): 1 turns EYE AA on now if it
// is off (as the hologram's press: ENGINE TAA goes off), 0 turns it off; a failed history pass
// stays refused. Sent at every start, so EYE AA no longer starts OFF.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEyeAaStart(void* on) {
    const auto v = reinterpret_cast<uintptr_t>(on);
    if (v > 1)
        return 5209;
    const uint32_t state = eyeAaNow() & 0xff;
    if ((v == 1 && state == eye_taa::off) || (v == 0 && state == eye_taa::on))
        pressEyeAa();
    OutputDebugStringA(v ? "EYE AA AT START: on\n" : "EYE AA AT START: off\n");
    return (eyeAaNow() & 0xff) == (v ? eye_taa::on : eye_taa::off) ? 0 : 5210;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWallBody(void* on) { return setLive(on, 0, 1, 5208, wallBodyOn, asBool); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyYankSlot(void* slot) {
    return setLive(slot, 0, combat_input::slots - 1, 5192, yankSlot, asU32);
}
// Update 37.2: COLOR / ATMOSPHERE (eye_color.hpp). Each sets one value and sends the whole
// look on the next frame (colorDirty). EXPOSURE takes tenths of EV + 20 (0..40), WARMTH and
// TINT their value + 100 (0..200).
namespace {
template <class T> DWORD setColor(void* in, uintptr_t low, uintptr_t high, DWORD refused, std::atomic<T>& to, intptr_t bias) {
    const auto value = reinterpret_cast<uintptr_t>(in);
    if (value < low || value > high)
        return refused;
    to = static_cast<T>(static_cast<intptr_t>(value) - bias);
    colorDirty = true;
    return 0;
}
} // namespace
extern "C" __declspec(dllexport) DWORD WINAPI SpidyColorFx(void* on) {
    const auto value = reinterpret_cast<uintptr_t>(on);
    if (value > 1)
        return 5195;
    colorFxOn = value != 0;
    colorDirty = true;
    return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyColorFilter(void* filter) {
    return setColor(filter, 0, eye_color::filters - 1, 5196, colorFilter, 0);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyColorStrength(void* percent) {
    return setColor(percent, eye_color::strengthLimits.lo, eye_color::strengthLimits.hi, 5197, colorStrength, 0);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyExposure(void* biased) {
    return setColor(biased, 0, 40, 5198, colorExposure, 20);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyContrast(void* percent) {
    return setColor(percent, eye_color::contrastLimits.lo, eye_color::contrastLimits.hi, 5199, colorContrast, 0);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidySaturation(void* percent) {
    return setColor(percent, eye_color::saturationLimits.lo, eye_color::saturationLimits.hi, 5200, colorSaturation, 0);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyVibrance(void* percent) {
    return setColor(percent, eye_color::vibranceLimits.lo, eye_color::vibranceLimits.hi, 5201, colorVibrance, 0);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWarmth(void* biased) {
    return setColor(biased, 0, 200, 5202, colorWarmth, 100);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyTint(void* biased) {
    return setColor(biased, 0, 200, 5203, colorTint, 100);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyVignette(void* percent) {
    return setColor(percent, eye_color::vignetteLimits.lo, eye_color::vignetteLimits.hi, 5204, colorVignette, 0);
}
// Update 38.5: 90s ANIMATED's CARTOON STRENGTH 0-100, OUTLINES 0/1, BANDS 3-5 (live, next frame).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyColorCartoon(void* percent) {
    return setColor(percent, eye_color::cartoonLimits.lo, eye_color::cartoonLimits.hi, 5340, colorCartoon, 0);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyColorOutlines(void* on) {
    return setColor(on, 0, 1, 5341, colorOutlines, 0);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyColorBands(void* bands) {
    return setColor(bands, eye_color::bandLimits.lo, eye_color::bandLimits.hi, 5342, colorBands, 0);
}
// Update 37.2: FOVEATED (EXPERIMENTAL): 0 OFF, 1 LIGHT, 2 BALANCED, 3 AGGRESSIVE; INNER SIZE
// 50-150 percent; FOVEATED DEBUG 0/1. Taken on the next frame (foveaDirty).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFoveated(void* level) {
    const DWORD code = setLive(level, 0, foveation::levels - 1, 5205, foveaLevelWant, asU32);
    if (!code)
        foveaDirty = true;
    return code;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFoveaInner(void* percent) {
    const DWORD code = setLive(percent, foveation::innerMin, foveation::innerMax, 5206, foveaInnerWant, asU32);
    if (!code)
        foveaDirty = true;
    return code;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFoveaDebug(void* on) {
    const DWORD code = setLive(on, 0, 1, 5207, foveaDebugWant, asBool);
    if (!code)
        foveaDirty = true;
    return code;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyAimCone(void* degrees) { return setLive(degrees, 0, 60, 5126, aimConeDegrees, asU32); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyLockStick(void* degrees) { return setLive(degrees, 0, 45, 5127, lockStickDegrees, asU32); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyLooseness(void* percent) { return setLive(percent, 50, 200, 5128, loosePercent, asU32); }
// Walls and climbing (hand_crawl.hpp).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWallCrawl(void* game) {
    const DWORD code = setLive(game, 0, 1, 5130, gameWallCrawl, asBool);
    if (!code)
        wallCrawlChanged = true;
    return code;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyHandClimb(void* on) {
    const DWORD code = setLive(on, 0, 1, 5131, climbOn, asBool);
    if (!code)
        handClimbAsked = reinterpret_cast<uintptr_t>(on) ? 1 : 0;
    return code;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyClimbPull(void* percent) {
    return setLive(percent, hand_crawl::Config::minPull, hand_crawl::Config::maxPull, 5132, climbPull, asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyClimbGrip(void* type) {
    return setLive(type, 0, hand_crawl::gripTypes - 1, 5133, climbGrip, asU32);
}
// Heal (the right stick down).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyHeal(void* on) { return setLive(on, 0, 1, 5140, healOn, asBool); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyHealHold(void* ms) { return setLive(ms, 50, 1000, 5141, healHoldMs, asU32); }
// The ring (health_ring::Place). ring_tilt arrives biased by 60 (0-120 for -60..60 degrees).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyRingFace(void* flat) { return setLive(flat, 0, 1, 5150, ringFlat, asBool); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyRingOffset(void* mm) {
    return setLive(mm, 0, static_cast<uintptr_t>(health_ring::Place::maxOffset), 5151, ringOffsetMm, asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyRingTilt(void* biased) {
    return setLive(biased, 0, 120, 5152, ringTiltDegrees, [](uintptr_t v) { return static_cast<int>(v) - 60; });
}
// Update 37: the MIRROR (vr_mirror.hpp). Off costs nothing; on, it is placed in front.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMirror(void* on) {
    const DWORD code = setLive(on, 0, 1, 5160, mirrorOn, asBool);
    if (!code && reinterpret_cast<uintptr_t>(on))
        mirrorRecentreAsked = true;
    return code;
}
// Update 38.5: EXTREME's VISOR DROPS 0/1 (default 0).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyBloodVisor(void* on) {
    return setLive(on, 0, 1, 5343, bloodVisor, asBool);
}
// Update 38.5 EXTRAS: INVINCIBLE, FLY MODE (0-3), FLY VIGNETTE, SYMBIOTE LOOK, PROMPT HANDLING.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyInvincible(void* on) {
    return setLive(on, 0, 1, 5344, invincibleOn, asBool);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFlyMode(void* speed) {
    return setLive(speed, 0, 3, 5345, flyModeNow, asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFlyVignette(void* on) {
    return setLive(on, 0, 1, 5346, flyVignetteOn, asBool);
}
// Update 38.5 FLY ROLL: 0 OFF, 1 COMFORT, 2 FULL; ROLL SPEED 30-180 degrees a second; AUTO LEVEL 0/1.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFlyRoll(void* mode) {
    return setLive(mode, 0, 2, 5350, flyRollNow, asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFlyRollSpeed(void* speed) {
    return setLive(speed, 30, 180, 5351, flyRollSpeedNow, asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFlyAutoLevel(void* on) {
    return setLive(on, 0, 1, 5352, flyAutoLevelNow, asBool);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidySymbioteLook(void* on) {
    return setLive(on, 0, 1, 5347, symbioteOn, asBool);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPromptHandling(void* handling) {
    return setLive(handling, 0, 1, 5348, promptHandlingNow, asU32);
}
// Update 38.5: GESTURE MOVES 0 OFF, 1 TRAINING, 2 ON (default 0).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGestureMoves(void* mode) {
    return setLive(mode, 0, gestures385::modes - 1, 5349, gestureMovesNow, asU32);
}
// Update 37: BLOOD SPLATTER intensity percent (0 OFF, 150+ BRUTAL, 200 EXTREME (38.5)).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyBloodSplatter(void* percent) {
    return setLive(percent, blood_splatter::minIntensity, blood_splatter::maxIntensity, 5174, bloodIntensity, asU32);
}
// Update 37: HOLO REUSE on/off (perf37.hpp). OFF rebuilds the hologram every frame, as before.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyHoloReuse(void* on) { return setLive(on, 0, 1, 5180, holoReuseOn, asBool); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMirrorDistance(void* cm) {
    return setLive(cm, vr_mirror::Config::minDistance, vr_mirror::Config::maxDistance, 5161, mirrorDistanceCm, asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMirrorSize(void* percent) {
    return setLive(percent, vr_mirror::Config::minSize, vr_mirror::Config::maxSize, 5162, mirrorSizePercent, asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMirrorSpot(void* spot) {
    return setLive(spot, 0, vr_mirror::spots - 1, 5163, mirrorSpot, asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMirrorAnchor(void* anchor) {
    const DWORD code = setLive(anchor, 0, vr_mirror::anchors - 1, 5164, mirrorAnchor, asU32);
    if (!code)
        mirrorRecentreAsked = true; // placed again in the new frame, in front of you
    return code;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMirrorStyle(void* style) {
    return setLive(style, 0, vr_mirror::styles - 1, 5165, mirrorStyle, asU32);
}
// Update 38: the COMBAT page (combat38.hpp), live; each refused with its own code (5301-5311)
// outside its values.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyCrawlCap(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::crawlCap].count - 1, 5301, combat38Values[combat38::crawlCap], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyZipMode(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::zipMode].count - 1, 5302, combat38Values[combat38::zipMode], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyAimAssist(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::aimAssist].count - 1, 5303, combat38Values[combat38::aimAssist], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyBallLook(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::ballLook].count - 1, 5304, combat38Values[combat38::ballLook], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPunchMode(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::punchMode].count - 1, 5305, combat38Values[combat38::punchMode], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyRingAnchor(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::ringAnchor].count - 1, 5306, combat38Values[combat38::ringAnchor], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyHoloOpen(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::holoOpen].count - 1, 5307, combat38Values[combat38::holoOpen], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWallTilt(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::wallTilt].count - 1, 5308, combat38Values[combat38::wallTilt], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWebRope(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::webRope].count - 1, 5309, combat38Values[combat38::webRope], asU32);
}
// Update 38 (SpidyVR fold-in): LUNGE (5310) and BALL FIRE (5311).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyLunge(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::lunge].count - 1, 5310, combat38Values[combat38::lunge], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyBallFire(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::ballFire].count - 1, 5311, combat38Values[combat38::ballFire], asU32);
}
// Update 38.1: AIM AXIS, RING LAYER, LUNGE FROM, LUNGE SPEED, LUNGE AIM, PUNCH ANGLE, BALL LEAD,
// BLOOD LOOK (5312-5319), live as the rest.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyAimAxis(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::aimAxis].count - 1, 5312, combat38Values[combat38::aimAxis], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyRingLayer(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::ringLayer].count - 1, 5313, combat38Values[combat38::ringLayer], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyLungeFrom(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::lungeFrom].count - 1, 5314, combat38Values[combat38::lungeFrom], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyLungeSpeed(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::lungeSpeed].count - 1, 5315, combat38Values[combat38::lungeSpeed], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyLungeAim(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::lungeAim].count - 1, 5316, combat38Values[combat38::lungeAim], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPunchAngle(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::punchAngle].count - 1, 5317, combat38Values[combat38::punchAngle], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyBallLead(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::ballLead].count - 1, 5318, combat38Values[combat38::ballLead], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyBloodLook(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::bloodLook].count - 1, 5319, combat38Values[combat38::bloodLook], asU32);
}
// Update 38.2: MELEE ANIM, GADGET KEYS, GRAB WEB, MIRROR LOOK, GADGET FEEL (5320-5324), live.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMeleeAnim(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::meleeAnim].count - 1, 5320, combat38Values[combat38::meleeAnim], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGadgetKeys(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::gadgetKeys].count - 1, 5321, combat38Values[combat38::gadgetKeys], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGrabWeb(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::grabWeb].count - 1, 5322, combat38Values[combat38::grabWeb], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMirrorLook(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::mirrorLook].count - 1, 5323, combat38Values[combat38::mirrorLook], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGadgetFeel(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::gadgetFeel].count - 1, 5324, combat38Values[combat38::gadgetFeel], asU32);
}
// Update 38.3: LOOK LOCK, STRIKE STEER, FAR WINDOWS, GADGET HINT (5325-5328), live (FAR WINDOWS
// reaches the view gates on the next frame).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyLookLock(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::lookLock].count - 1, 5325, combat38Values[combat38::lookLock], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyStrikeSteer(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::strikeSteer].count - 1, 5326, combat38Values[combat38::strikeSteer], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFarWindowsOption(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::farWindows].count - 1, 5327, combat38Values[combat38::farWindows], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGadgetHint(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::gadgetHint].count - 1, 5328, combat38Values[combat38::gadgetHint], asU32);
}
// Version 0.0.2: AIM STRENGTH and BALL HIT (5329-5330), live (both for BALL LOOK OURS).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyAimStrength(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::aimStrength].count - 1, 5329, combat38Values[combat38::aimStrength], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyBallHit(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::ballHit].count - 1, 5330, combat38Values[combat38::ballHit], asU32);
}
// Version 0.0.3: ROPE GRAB .. SHOW STATUS (5331-5337), live (each takes effect on the next frame).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyRopeGrab(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::ropeGrab].count - 1, 5331, combat38Values[combat38::ropeGrab], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyRopeStrength(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::ropeStrength].count - 1, 5332, combat38Values[combat38::ropeStrength], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWallStick(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::wallStick].count - 1, 5333, combat38Values[combat38::wallStick], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyAutoPerf(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::autoPerf].count - 1, 5334, combat38Values[combat38::autoPerf], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPerfTarget(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::perfTarget].count - 1, 5335, combat38Values[combat38::perfTarget], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPerfFloor(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::perfFloor].count - 1, 5336, combat38Values[combat38::perfFloor], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPerfStatus(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::perfStatus].count - 1, 5337, combat38Values[combat38::perfStatus], asU32);
}
// Version 0.0.4 (Update 38.8)
extern "C" __declspec(dllexport) DWORD WINAPI SpidyShooterType(void* v) {
    const DWORD r = setLive(v, 0, combat38::optionInfo[combat38::shooterType].count - 1, 5338,
                            combat38Values[combat38::shooterType], asU32);
    if (!r)
        heroOnly(combat38::shooterType); // Version 0.0.6: one hero toggle at a time
    return r;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMirrorView(void* v) {
    const DWORD r =
        setLive(v, 0, combat38::optionInfo[combat38::mirrorView].count - 1, 5339, combat38Values[combat38::mirrorView], asU32);
    // 0.0.7 MIRROR PERSIST FIX: the wish reaches the probe at once (the launcher sends live.txt's before
    // SpidyXrStart): the view is only made with the eyes, before the session loop's quarter-second
    // SpidyMirrorViewWant, which kept a live.txt "mirror_view = test" too late as well.
    if (!r)
        SpidyMirrorViewWant(c38(combat38::mirrorView));
    return r;
}
// Version 0.0.5 (Update 38.9)
extern "C" __declspec(dllexport) DWORD WINAPI SpidyHeroHeight(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::heroHeight].count - 1, 5361, combat38Values[combat38::heroHeight], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyHeroDamage(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::heroDamage].count - 1, 5362, combat38Values[combat38::heroDamage], asU32);
}
// Version 0.0.5 (11:24): PUNCH SENSITIVITY, SFX VOLUME, SUPERSONIC, SUPERSONIC SPEED (all live)
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPunchSense(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::punchSense].count - 1, 5363, combat38Values[combat38::punchSense], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidySfxVolume(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::sfxVolume].count - 1, 5364, combat38Values[combat38::sfxVolume], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidySupersonic(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::supersonic].count - 1, 5365, combat38Values[combat38::supersonic], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidySupersonicSpeed(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::supersonicSpeed].count - 1, 5366, combat38Values[combat38::supersonicSpeed], asU32);
}
// Version 0.0.6 (Update 39.0): DAREDEVIL / HULK ABILITIES (one hero toggle at a time) and DAREDEVIL PUNCH
extern "C" __declspec(dllexport) DWORD WINAPI SpidyDaredevil(void* v) {
    const DWORD r = setLive(v, 0, combat38::optionInfo[combat38::daredevil].count - 1, 5367,
                            combat38Values[combat38::daredevil], asU32);
    if (!r)
        heroOnly(combat38::daredevil);
    return r;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyHulk(void* v) {
    const DWORD r = setLive(v, 0, combat38::optionInfo[combat38::hulk].count - 1, 5368, combat38Values[combat38::hulk], asU32);
    if (!r)
        heroOnly(combat38::hulk);
    return r;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyDaredevilPunch(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::daredevilPunch].count - 1, 5369, combat38Values[combat38::daredevilPunch], asU32);
}
// Version 0.0.62
extern "C" __declspec(dllexport) DWORD WINAPI SpidyHitIndicator(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::hitIndicator].count - 1, 5370, combat38Values[combat38::hitIndicator], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyHandFlips(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::handFlips].count - 1, 5371, combat38Values[combat38::handFlips], asU32);
}
// LENS FLARES: kept for the next start (run_game_vr writes the game's setting); the value only
// shows on the hologram while VR runs.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyLensFlares(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::lensFlares].count - 1, 5372, combat38Values[combat38::lensFlares], asU32);
}
// Version 0.0.62 PUNCH HITS / ENEMY TRACKING (the SpidyVR comparison), live.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPunchHits(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::punchHits].count - 1, 5373, combat38Values[combat38::punchHits], asU32);
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFoeTrack(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::foeTrack].count - 1, 5374, combat38Values[combat38::foeTrack], asU32);
}
// Version 0.0.62 BEACONS IN VISION (DAREDEVIL SONAR), live.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyBeaconsVision(void* v) {
    return setLive(v, 0, combat38::optionInfo[combat38::beaconsVision].count - 1, 5375, combat38Values[combat38::beaconsVision], asU32);
}
// Version 0.0.7 HERO PROFILES: BATMAN / WOLVERINE / SUPERMAN / PUNISHER (one hero toggle at a time),
// BATARANG CHAIN, EASY RELOAD, HOLSTER HANDS, BATON HOLSTERS, BATON LOOK, live.
namespace {
DWORD setHero007(void* v, uint32_t option, DWORD refused) {
    const DWORD r = setLive(v, 0, combat38::optionInfo[option].count - 1, refused, combat38Values[option], asU32);
    if (!r && combat38::isHeroToggle(option))
        heroOnly(option);
    return r;
}
} // namespace
extern "C" __declspec(dllexport) DWORD WINAPI SpidyBatman(void* v) { return setHero007(v, combat38::batman, 5376); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWolverine(void* v) { return setHero007(v, combat38::wolverine, 5377); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidySuperman(void* v) { return setHero007(v, combat38::superman, 5378); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPunisher(void* v) { return setHero007(v, combat38::punisher, 5379); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyBatarangChain(void* v) { return setHero007(v, combat38::batarangChain, 5380); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyEasyReload(void* v) { return setHero007(v, combat38::easyReload, 5381); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyHolsterHands(void* v) { return setHero007(v, combat38::holsterHands, 5382); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyBatonHolsters(void* v) { return setHero007(v, combat38::batonHolsters, 5383); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyBatonLook(void* v) { return setHero007(v, combat38::batonLook, 5384); }
// Version 0.0.7 BATARANG STYLE (the model itself is read on its first use: mesh_model::Library).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyBatarangStyle(void* v) { return setHero007(v, combat38::batarangStyle, 5385); }
// Version 0.0.7 GUN MODELS (PUNISHER; the models are read on their first use: mesh_model::Library).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGunModels(void* v) { return setHero007(v, combat38::gunModels, 5386); }
// Version 0.0.7 CAPTAIN AMERICA (a hero toggle) and his shield's SHIELD ARM, SHIELD DAMAGE, SHIELD
// CHAIN, SHIELD BLOCK (EXPERIMENTAL, CANDIDATE) and SHIELD LOOK, live.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyCaptainAmerica(void* v) { return setHero007(v, combat38::captainAmerica, 5387); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyShieldArm(void* v) { return setHero007(v, combat38::shieldArm, 5388); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyShieldDamage(void* v) { return setHero007(v, combat38::shieldDamage, 5389); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyShieldChain(void* v) { return setHero007(v, combat38::shieldChain, 5390); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyShieldBlock(void* v) { return setHero007(v, combat38::shieldBlock, 5391); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyShieldLook(void* v) { return setHero007(v, combat38::shieldLook, 5392); }
// Version 0.0.72 (TEST): THOR, GREEN LANTERN (hero toggles) and the 0.0.72 list's options, live.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyThor(void* v) { return setHero007(v, combat38::thor, 5393); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyThorLightning(void* v) { return setHero007(v, combat38::thorLightning, 5394); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyThorStorm(void* v) { return setHero007(v, combat38::thorStorm, 5395); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyThorChain(void* v) { return setHero007(v, combat38::thorChain, 5396); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGreenLantern(void* v) { return setHero007(v, combat38::greenLantern, 5397); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGlFlight(void* v) { return setHero007(v, combat38::glFlight, 5398); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGlFist(void* v) { return setHero007(v, combat38::glFist, 5399); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGlBeam(void* v) { return setHero007(v, combat38::glBeam, 5400); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGlBubble(void* v) { return setHero007(v, combat38::glBubble, 5401); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGlChain(void* v) { return setHero007(v, combat38::glChain, 5402); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGlCharge(void* v) { return setHero007(v, combat38::glCharge, 5403); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidySpiderClimbOnly(void* v) { return setHero007(v, combat38::spiderClimbOnly, 5404); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyUppercut(void* v) { return setHero007(v, combat38::uppercut, 5405); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidySmokeBlind(void* v) { return setHero007(v, combat38::smokeBlind, 5406); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyDdGrapple(void* v) { return setHero007(v, combat38::ddGrapple, 5407); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyDdParry(void* v) { return setHero007(v, combat38::ddParry, 5408); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyIronHud(void* v) { return setHero007(v, combat38::ironHud, 5409); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPunisherRage(void* v) { return setHero007(v, combat38::punisherRage, 5410); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyReputation(void* v) { return setHero007(v, combat38::reputation, 5411); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidySurvival(void* v) { return setHero007(v, combat38::survival, 5412); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPunisherSuit(void* v) { return setHero007(v, combat38::punisherSuit, 5413); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyNaturalInvincible(void* v) { return setHero007(v, combat38::naturalInvincible, 5414); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyHulkRage(void* v) { return setHero007(v, combat38::hulkRage, 5415); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyRageBlood(void* v) { return setHero007(v, combat38::rageBlood, 5416); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyRageTint(void* v) { return setHero007(v, combat38::rageTint, 5417); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFireDouse(void* v) { return setHero007(v, combat38::fireDouse, 5418); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyShieldSlam(void* v) { return setHero007(v, combat38::shieldSlam, 5419); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPunisherHook(void* v) { return setHero007(v, combat38::punisherHook, 5420); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyClawClimb(void* v) { return setHero007(v, combat38::clawClimb, 5421); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyNpcShove(void* v) { return setHero007(v, combat38::npcShove, 5422); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyFaction(void* v) { return setHero007(v, combat38::faction, 5423); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyHeroSuit(void* v) { return setHero007(v, combat38::heroSuit, 5424); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyTestUnlocks(void* v) { return setHero007(v, combat38::testUnlocks, 5425); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyUltimates(void* v) { return setHero007(v, combat38::ultimates, 5426); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPerkBranch(void* v) { return setHero007(v, combat38::perkBranch, 5427); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyOwnSuit(void* v) { return setHero007(v, combat38::ownSuit, 5428); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPerchTakedown(void* v) { return setHero007(v, combat38::perchTakedown, 5429); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyTakedownLight(void* v) { return setHero007(v, combat38::takedownLight, 5430); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyPunisherVan(void* v) { return setHero007(v, combat38::punisherVan, 5431); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyThorHammer(void* v) { return setHero007(v, combat38::thorHammer, 5432); }
// Version 0.0.72 GAME BODY (EXPERIMENTAL): 0 off (the body as before), 1 on (strict checks first), live.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyGameBody(void* v) { return setHero007(v, combat38::gameBody, 5433); }
// Version 0.0.72 build 6: the THOR crash bisect keys (live.txt thor_hammer_model / thor_flight).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyThorHammerModel(void* v) { return setHero007(v, combat38::thorHammerModel, 5434); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyThorFlight(void* v) { return setHero007(v, combat38::thorFlight, 5435); }
// Build 9 SUITS: SUIT BODY (live.txt suit_body = arms / full_body / off).
extern "C" __declspec(dllexport) DWORD WINAPI SpidySuitBody(void* v) { return setHero007(v, combat38::suitBody, 5436); }
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMirrorRecentre(void*) {
    mirrorRecentreAsked = true;
    return 0;
}
// The wrist hologram (Update 14, live.txt hud): 0 off, 1 on (which also
// clears a failure that turned it off); refused (3965) otherwise.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWristHud(void* on) {
    const auto value = reinterpret_cast<uintptr_t>(on);
    if (value > 1)
        return 3965;
    if (value) {
        wristHudFault = false;
        wristMenuFault = false;
    }
    wristHudOn = value != 0;
    return 0;
}
// Its wrist (live.txt hud_hand): 0 the left, toggled by the right hand; 1 the
// right, toggled by the left. Refused (3966) otherwise.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWristHudHand(void* hand) {
    const auto value = reinterpret_cast<uintptr_t>(hand);
    if (value > 1)
        return 3966;
    wristHudHand = static_cast<uint32_t>(value);
    return 0;
}
// Its size (live.txt hud_scale), percent of 18 by 22 cm: 50-200; refused
// (3967) otherwise.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWristHudScale(void* percent) {
    const auto value = reinterpret_cast<uintptr_t>(percent);
    if (value < 50 || value > 200)
        return 3967;
    wristHudScalePercent = static_cast<uint32_t>(value);
    return 0;
}
// Update 17: the HOLOGRAM page's. Its height over the light (live.txt
// hud_height), cm 0-25; refused (3972) otherwise. The size is
// SpidyWristHudScale's.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWristHudHeight(void* cm) {
    const auto value = reinterpret_cast<uintptr_t>(cm);
    if (value > 25)
        return 3972;
    wristHudHeightCm = static_cast<uint32_t>(value);
    return 0;
}
// Its pages' text size (live.txt hud_text), percent 75-175; refused (3973).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWristHudText(void* percent) {
    const auto value = reinterpret_cast<uintptr_t>(percent);
    if (value < 75 || value > 175)
        return 3973;
    wristHudTextPercent = static_cast<uint32_t>(value);
    return 0;
}
// Its colour theme (live.txt hud_theme): 0 Spider-Man, 1 high contrast, 2
// deutan/protan safe, 3 tritan safe, 4 monochrome, 5 (Update 36) 2000s
// Raimi; refused (3974) otherwise.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWristHudTheme(void* theme) {
    const auto value = reinterpret_cast<uintptr_t>(theme);
    if (value >= static_cast<uintptr_t>(wrist_hud::themes))
        return 3974;
    wristHudTheme = static_cast<uint32_t>(value);
    return 0;
}
// Its text colour (live.txt hud_text_color): 0 the theme's, 1 white, 2
// yellow, 3 cyan; refused (3975) otherwise.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWristHudTextColor(void* color) {
    const auto value = reinterpret_cast<uintptr_t>(color);
    if (value > 3)
        return 3975;
    wristHudTextColor = static_cast<uint32_t>(value);
    return 0;
}
// Update 16: the swing aim reticles (live.txt reticles): 1 shown, 0 hidden;
// refused (3971) otherwise. Only the drawing: aiming and swinging are the same.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyReticles(void* shown) {
    const auto value = reinterpret_cast<uintptr_t>(shown);
    if (value > 1)
        return 3971;
    reticlesShown = static_cast<uint32_t>(value);
    return 0;
}
// Update 18: the webs' look (live.txt web_style): 0 90s, 1 MESSY, 2 MODERN,
// 3 SYMBIOTE, 4 ENERGY; refused (3976) otherwise. Only the picture; each
// style keeps its own shade (SpidyWebColor).
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWebStyle(void* style) {
    const auto value = reinterpret_cast<uintptr_t>(style);
    if (value >= static_cast<uintptr_t>(web_style::count))
        return 3976;
    webStyleNow = static_cast<uint32_t>(value);
    return 0;
}
// A style's shade (live.txt web_color, web_color_<style>): bits 0-3 the
// shade in that style's list (web_style::shadeName), bits 4-7 the style + 1
// (0: the style chosen now); refused (3977) for a shade or style there is not.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyWebColor(void* color) {
    const auto value = reinterpret_cast<uintptr_t>(color);
    const uintptr_t which = value >> 4, shade = value & 15;
    if (value > 0xff || which > static_cast<uintptr_t>(web_style::count))
        return 3977;
    const uint32_t style = which ? static_cast<uint32_t>(which - 1)
                                 : std::min<uint32_t>(webStyleNow.load(), web_style::count - 1);
    if (shade >= static_cast<uintptr_t>(web_style::shades(static_cast<int>(style))))
        return 3977;
    webShades[style] = static_cast<uint32_t>(shade);
    return 0;
}
// Update 19: the MUSIC page's volume (live.txt music_volume), percent 0-100;
// refused (3978) otherwise. Only the mod's music: never the game's sound.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMusicVolume(void* percent) {
    const auto value = reinterpret_cast<uintptr_t>(percent);
    if (value > 100)
        return 3978;
    musicVolumeNow = static_cast<uint32_t>(value);
    return 0;
}
// Its visualizer (live.txt music_visualizer): 0 ORIGINAL, 1 BARS, 2 FRACTAL
// BLOOM, 3 TUNNEL, 4 STARBURST; refused (3979) otherwise.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMusicVisualizer(void* kind) {
    const auto value = reinterpret_cast<uintptr_t>(kind);
    if (value >= static_cast<uintptr_t>(music::visualizers))
        return 3979;
    musicVisualizerNow = static_cast<uint32_t>(value);
    return 0;
}
// The song to start from (live.txt music_track): 1 the first in A-Z order,
// 0 none; picked once the songs are found, never played by itself; refused
// (3980) past 65535.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyMusicTrack(void* song) {
    const auto value = reinterpret_cast<uintptr_t>(song);
    if (value > 0xffff)
        return 3980;
    musicTrackStart = static_cast<int32_t>(value) - 1;
    if (value)
        musicTrackNow = static_cast<int32_t>(value) - 1;
    return 0;
}
// Update 15: what the hologram's pages have set, for the launcher to keep
// (tools/live_settings.py decode_hud_settings): 0 the webs (diameter in tenths
// of a millimetre in bits 0-9, speed m/s in 10-20), 1 the graphics (aa bit 0,
// history 1-7, reach 8-16, monitor 17-24), 2 the next launch's eye supersample
// (0 none chosen), 3 the settings (1 << wrist_hud::Control) the pages changed
// since the last 3, cleared by reading, 4 (Update 16) the eye sharpening
// 0-200 (bits 0-7; Update 16-20: 0-100 in 0-6) and the aim reticles shown (bit 8;
// was 7), 5 (Update 17) the
// hologram: height cm (bits 0-4), size percent (5-12), text percent (13-20),
// theme (21-23), text colour (24-25), 6 (Update 18) the webs' look: style
// (bits 0-2) and each style's shade (3 bits each from bit 3, in web_style's
// order), 7 (Update 19) the music: volume (bits 0-6), visualizer (7-9), the
// song + 1 (10-25, 0 none), 8 and 9 (Update 24) the session FPS (FpsSession:
// AVG tenths 0-10 and seconds tracked 11-28; HIGH tenths 0-10, LOW 11-21).
// Bit 30 is always set; 3970 for another selector.
extern "C" __declspec(dllexport) DWORD WINAPI SpidyHudSettings(void* selector) {
    constexpr DWORD marker = 1u << 30;
    switch (reinterpret_cast<uintptr_t>(selector)) {
    case 0:
        return marker | std::min<DWORD>(webRadiusTenthsMm.load() * 2, 1023) |
               std::min<DWORD>(webSpeedMs.load(), 2047) << 10;
    case 1:
        return marker | std::min<DWORD>(native_gpu::eyeAA(), 1) | std::min<DWORD>(native_gpu::eyeHistory(), 127) << 1 |
               std::min<DWORD>(SpidyImpostorReachNow(), 511) << 8 |
               std::min<DWORD>(native_gpu::monitorRateNow(), 255) << 17;
    case 2:
        return marker | std::min<DWORD>(nextSupersample.load(), 1023);
    case 3:
        // The 20 settings' bits, and Update 19's song (20), music volume (27) and visualizer (28).
        // Version 0.0.62: LENS FLARES (29), kept for the next start. 0.0.7: MIRROR VIEW (21), the same.
        return marker | (wristChanged.exchange(0) & (0xfffffu | 1u << 20 | 1u << 21 | 1u << 27 | 1u << 28 | 1u << 29));
    case 4: // Update 21: sharpening 0-200 in bits 0-7, the reticles shown in bit 8 (Update 16: 0-6 and 7)
        return marker | std::min<DWORD>(native_gpu::eyeSharpen(), 255) | (reticlesShown.load() ? 1u : 0u) << 8;
    case 5: // Update 17: the hologram's height, size, text size, theme and text colour
        return marker | std::min<DWORD>(wristHudHeightCm.load(), 31) | std::min<DWORD>(wristHudScalePercent.load(), 255) << 5 |
               std::min<DWORD>(wristHudTextPercent.load(), 255) << 13 | std::min<DWORD>(wristHudTheme.load(), 7) << 21 |
               std::min<DWORD>(wristHudTextColor.load(), 3) << 24;
    case 6: { // Update 18: the web style (0-2) and each style's shade (3 bits each from 3)
        DWORD code = marker | std::min<DWORD>(webStyleNow.load(), 7);
        for (int i = 0; i < web_style::count; ++i)
            code |= std::min<DWORD>(webShades[i].load(), 7) << (3 + 3 * i);
        return code;
    }
    case 7: // Update 19: music volume (bits 0-6), visualizer (7-9), the song + 1 (10-25, 0 none)
        return marker | std::min<DWORD>(musicVolumeNow.load(), 100) | std::min<DWORD>(musicVisualizerNow.load(), 7) << 7 |
               static_cast<DWORD>(std::clamp(musicTrackNow.load() + 1, 0, 0xffff)) << 10;
    case 8: // Update 24: the session FPS: AVG in tenths (bits 0-10, 0 none), the seconds tracked (11-28)
        return marker | std::min<DWORD>(sessionFpsAvg.load(), 2047) | std::min<DWORD>(sessionFpsSeconds.load(), 262143) << 11;
    case 9: // Update 24: the session FPS: HIGH in tenths (bits 0-10), LOW (11-21); 0 none
        return marker | std::min<DWORD>(sessionFpsHigh.load(), 2047) | std::min<DWORD>(sessionFpsLow.load(), 2047) << 11;
    case 10: // Version 0.0.62: LENS FLARES (0 GAME, 1 OFF IN VR), for live.txt and the next start;
             // 0.0.7: MIRROR VIEW (bit 1: 0 OFF, 1 TEST), the same (an older launcher reads bit 0 only)
        return marker | std::min<DWORD>(c38(combat38::lensFlares), 1) | std::min<DWORD>(c38(combat38::mirrorView), 1) << 1;
    default:
        return 3970;
    }
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyXrKeepAlive(void*) {
    if (stopRequested)
        return 1102;
    keepAliveDeadline = GetTickCount64() + 5000;
    return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI SpidyXrStop(void*) {
    std::lock_guard life(lifecycle);
    stopRequested = true;
    if (!worker)
        return 0;
    const auto status = WaitForSingleObject(worker, 15000);
    return status == WAIT_OBJECT_0 ? 0 : 1101;
}
