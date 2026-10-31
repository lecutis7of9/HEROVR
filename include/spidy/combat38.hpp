#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 38 (combat): the COMBAT hologram page's live settings and the small rules
// behind them, kept here (no Windows) so tests/core_tests.cpp checks every one.
//   crawl_cap   the head's crouch cap: only under a ceiling low enough to force one
//   zip_mode    which punches go out as the zip strike
//   aim_assist  the web ball's aim toward the nearest enemy in a cone
//   ball_look   our own glowing web ball over the game's
//   punch_mode  which hand hits count: fast punches, touching an enemy, or both
//   ring_anchor the health ring on the newest tracked wrist, or as 37.2 drew it
//   holo_open   how the wrist hologram opens (TAP by default; F8 always opens it)
//   wall_tilt   the wall-crawl tilt's reset on death and its cap off a wall
//   web_rope    the swing web as a thick rope, or the 37.2 fibre look
//   lunge       a SpidyVR lunge as the game's own web strike button, or SpidyVR's flight
//   ball_fire   the web ball on the trigger's press (SpidyVR), or on its release (37.2)
// Update 38.1 (run38/DIAGNOSIS.md), each fix with its plan B:
//   aim_axis    the aim of every web ball, glob, assist, punch and blood: the hand's -Y (the
//               swing webs' webShooter -Z), the controller's aim pose, or 38's grip -Z
//   ring_layer  the ring and hologram at the newest hand, hidden in a stale picture, or the image's
//   lunge_from  the nearest distance a swing lunges from (shorter ones are punches)
//   lunge_speed the swing speed that makes a lunge
//   lunge_aim   STEADY: a lunge only at a visible, looked-at, held target; 38's looser one
//   punch_angle how forward a fast swing must be to count (cos)
//   ball_lead   the web ball's lead/bend only with a target, or always (its RB late by the lead)
//   blood_look  38.1's bigger blood at the enemy, or 37's small hand spray
// Update 38.2 (run381/DIAGNOSIS.md; each 38.1 value kept as plan B, the new one first):
//   holo_open   FOREARM: a click by the other hand on the forearm opens it (no look)
//   ring_layer  HOLD: the ring and hologram never hide (a stale picture keeps the last pose)
//   ring_anchor WRIST: on the drawn wrist's web shooter (the grip, not the aim pose, pulled
//               back to where the game's arm reaches)
//   punch_mode  TARGETED: an attack only with an enemy in reach or locked, thrown at him
//   aim_assist  SMART: by angle weighted by distance, at centre mass
//   blood_look  BRUTAL: bigger, lasting, more drops and a coat on the hand
//   melee_anim  PUNCH ONLY: one X tap per blow, spaced past the combo (CANDIDATE: the game's
//               kick variants come from X chained within its combo window)
//   gadget_keys 38.2: Grabber RB+B (the player's video: RB+B was the Grabber), the others
//               CANDIDATE; GAME DEFAULT; CUSTOM (live.txt *_slot, 38.1's)
//   grab_web    ENEMY: a grab web that meets an enemy's body sticks to him; pull back yanks
//   mirror_look SUIT: the mirror draws the hero's own solved skeleton in the suit's colours
//   gadget_feel EASY: simpler, looser gadget moves with a buzz on each; 38.1's
// Update 38.3 (run382/DIAGNOSIS.md): ball_look GAME ONLY is now the default (index 0); grab_web
// ASSIST (a 12 degree cone, EXACT is 38.2's capsule); and
//   look_lock    QUICK: the lock follows a deliberate head turn (0.15 s, 8 degrees nearer)
//   strike_steer TO LOCK: the stick at the locked enemy while the web strike's Y goes (CANDIDATE)
//   far_windows  2X: the eyes' detail reach (CONFIRMED write) and the window impostors' reach (CANDIDATE)
//   gadget_hint  ON: the gadget's name at the wrist, and the Sonic pose after a missed one
// Version 0.0.2 (Update 38.6, run001/DIAGNOSIS.md): ball_look OURS is the default (index 0, the
// mod's own ball from the pulled wrist, no game ball: our_ball.hpp); WRIST, GLOB and GAME ONLY
// stay as plan B (each one index later). And
//   aim_strength STRONG: OURS's cone, homing, lead and reach (our_ball::tuneOf), OFF..MAX
//   ball_hit     OURS ONLY: a hit is our splash and buzz; +GAME WEB (experimental) also asks the
//                game for its own ball at the hit (CANDIDATE)
// Every value takes effect at once (no restart). Index 0 of each is the default.
#include "math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace spidy::combat38 {

enum Option : uint32_t {
    crawlCap = 0, zipMode, aimAssist, ballLook, punchMode, ringAnchor, holoOpen, wallTilt, webRope, lunge, ballFire,
    // Update 38.1
    aimAxis, ringLayer, lungeFrom, lungeSpeed, lungeAim, punchAngle, ballLead, bloodLook,
    // Update 38.2
    meleeAnim, gadgetKeys, grabWeb, mirrorLook, gadgetFeel,
    // Update 38.3
    lookLock, strikeSteer, farWindows, gadgetHint,
    // Version 0.0.2 (Update 38.6)
    aimStrength, ballHit,
    // Version 0.0.3 (Update 38.7): each experimental one OFF by default (value 0)
    ropeGrab, ropeStrength, wallStick, autoPerf, perfTarget, perfFloor, perfStatus,
    // Version 0.0.4 (Update 38.8): IRON MAN ABILITIES (repulsor.hpp) and the MIRROR VIEW TEST (mirror_view.hpp)
    shooterType, mirrorView,
    // Version 0.0.5 (Update 38.9): HERO HEIGHT (hero_height.hpp, live) and DAMAGE TEST (sm2_damage.hpp,
    // experimental, OFF by default)
    heroHeight, heroDamage,
    // Version 0.0.5 (the 0.0.4 headset test): PUNCH SENSITIVITY, SFX VOLUME, SUPERSONIC and its speed
    punchSense, sfxVolume, supersonic, supersonicSpeed,
    // Version 0.0.6 (Update 39.0): DAREDEVIL and HULK ABILITIES (hero_modes.hpp; one hero toggle on at
    // a time with IRON MAN's shooterType) and DAREDEVIL's punch multiplier
    daredevil, hulk, daredevilPunch,
    // Version 0.0.62: HIT INDICATOR (hit_marks.hpp, OFF by default), HAND FLIPS (ON: 0.0.6's
    // behaviour) and LENS FLARES (GAME: the game's own graphics setting; OFF IN VR: RESTART)
    hitIndicator, handFlips, lensFlares,
    // Version 0.0.62 (the SpidyVR comparison): PUNCH HITS (GAME + INSTANT: SpidyVR's blow on the
    // contact frame, as well as the game's move) and ENEMY TRACKING (LIVE: world positions, 30 a second)
    punchHits, foeTrack,
    // Version 0.0.62: BEACONS IN VISION (DAREDEVIL SONAR keeps bright coloured beacons and markers)
    beaconsVision,
    // Version 0.0.7 HERO PROFILES (hero007.hpp, holster.hpp): BATMAN, WOLVERINE, SUPERMAN and PUNISHER
    // (hero toggles: one hero at a time with the 0.0.6 ones), BATARANG CHAIN, EASY RELOAD, HOLSTER
    // HANDS, DAREDEVIL's BATON HOLSTERS and BATON LOOK. Each OFF / its 0.0.62 behaviour by default, live.
    batman, wolverine, superman, punisher, batarangChain, easyReload, holsterHands, batonHolsters, batonLook,
    // BATARANG STYLE (mesh_model.hpp): DEFAULT, or a dropped-in Sketchfab model (else DEFAULT).
    batarangStyle,
    // GUN MODELS: PUNISHER's pistols and sawed-off as the built-in looks (0) or the converted models.
    gunModels,
    // Version 0.0.7 CAPTAIN AMERICA (captain.hpp): the hero toggle, SHIELD ARM, SHIELD DAMAGE, SHIELD
    // CHAIN, SHIELD BLOCK (EXPERIMENTAL, CANDIDATE) and SHIELD LOOK. Each OFF / its first value by default, live.
    captainAmerica, shieldArm, shieldDamage, shieldChain, shieldBlock, shieldLook,
    // Version 0.0.72 (TEST, hero072.hpp): THOR and GREEN LANTERN (hero toggles, OFF), their powers
    // (each ON by default; only matter while that hero plays) and the 0.0.72 list's rules. Each live.
    thor, thorLightning, thorStorm, thorChain,
    greenLantern, glFlight, glFist, glBeam, glBubble, glChain, glCharge,
    spiderClimbOnly, uppercut, smokeBlind, ddGrapple, ddParry, ironHud, punisherRage, reputation, survival,
    punisherSuit, naturalInvincible, hulkRage,
    // Version 0.0.72 (the user, 8:28 - 8:35): RAGE BLOOD, RAGE TINT, FIRE DOUSE, SHIELD SLAM, GRAPPLE HOOK, CLAW CLIMB.
    rageBlood, rageTint, fireDouse, shieldSlam, punisherHook, clawClimb,
    // Version 0.0.72 (the user, 8:28 - 8:35): SHOVE, ENEMY FACTION.
    npcShove, faction,
    // Version 0.0.72 (the user, 8:28 - 8:35): HERO SUIT, UNLOCK ALL SUITS.
    heroSuit, testUnlocks,
    // Version 0.0.72 (the user, 8:46, 8:47): ULTIMATES and HERO PERKS (hero072_progress.hpp).
    ultimates, perkBranch,
    // Version 0.0.72 (the user, 8:59): USE MY OWN OR GAME SUIT, the alternate suits' fallback.
    ownSuit,
    // Version 0.0.72 (the user, 9:31, 9:32): BATMAN's PERCH TAKEDOWN and its GREEN / RED light.
    perchTakedown, takedownLight,
    punisherVan, // Version 0.0.72 EXPERIMENTAL: the Punisher van (van072.hpp); 0.0.73 test 3: ON first (the default)
    thorHammer,  // Version 0.0.72 THOR HAMMER: which Mjolnir (mesh_model::hammerSlot), BOOOOP by default
    gameBody,    // Version 0.0.72 GAME BODY (EXPERIMENTAL): strict checks (native_game_body.hpp), off by default
    // Version 0.0.72 build 6 (THOR crash bisect, the user 9:37): THOR HAMMER MODEL and THOR FLIGHT, ON by default.
    thorHammerModel, thorFlight,
    // Build 9 SUITS (suit_wear.hpp): SUIT BODY - the worn suit model in first person (ARMS, FULL BODY, OFF).
    suitBody,
    // Version 0.0.73 (fix 17): CAPTAIN AMERICA's SUPER SPRINT (grapple073::ArmPump), ON by default.
    capSprint,
    // 0.0.73 test 2 (fixes 9, 13): CULL MARGIN, the game view's degrees past the head's lens, 10 by default
    cullMargin,
    // 0.0.73 test 2 (fix 21): SHADOW MATCH (CANDIDATE), both eyes' cascades fitted to one lens; test 3: ON by default
    // (value 0 ON, 1 OFF - shadowMatchOn)
    shadowMatch,
    // Version 0.0.73 test 2 HOLO CALIBRATION (fix 25): HOLO checklist with belt, back and hip touches, or CLASSIC
    calibLook,
    // Version 0.0.73 test 2 FLIGHT POSE (fix 27): LEAN by speed (fly_lean.hpp) or UPRIGHT (0.0.72)
    flyPose,
    // 0.0.73 test 3 WEAPON COLLISION (prop_bounce.hpp): thrown and held weapons meet walls, cars and the ground, ON
    weaponCollision,
    // 0.0.73 test 3 LIGHT HOLD (eye_history.hpp): steadier street lights - EYE AA's step no larger than the
    // history can average, and bright history held; ON by default, STRONG (no step, full hold), OFF = test 2
    lightHold,
    // Version 0.0.73 test 3 VAN SCREEN: the Punisher van's dash screen (van_screen.hpp) ON or OFF
    vanScreen,
    // 0.0.73 test 3 PALM BLAST: IRON MAN's blast straight out of the open palm (PALM, on) or along the hand (AIM, fix 15)
    palmBlast,
    // 0.0.73 test 3 HOLO ROSTER (fix 28): the wrist ROSTER row opens the hero tiles on the hologram (HOLO TILES, on) or the big popup (CLASSIC)
    heroMenu,
    // 0.0.73 test 3 REP PAGE: each rep change logged for the hologram's REP tab (ON) or not (OFF)
    repLog,
    // 0.0.73 test 3 VAN PEDALS: WHEEL HAND (on: a trigger drives only while its hand grips the wheel) or CLASSIC (0.0.72)
    vanPedals,
    // 0.0.73 IMAGE: SPIDYVR (sharp: no FXAA, no sharpen, the history read Catmull-Rom, mip bias 0 at the
    // next start) or HERO VR CLASSIC (test 3: FXAA, sharpen 140, bilinear history, mip bias 0.5)
    image,
    // 0.0.73 test 4 IMAGE STYLE, shown as GAME TAA (image_style.hpp): SPIDYVR (the game's own TAA in each
    // eye, as SpidyVR) or OFF (IMAGE's look: EYE AA history, FXAA and sharpening as IMAGE sets; the default since
    // the 0.0.73 release - index 0 stays SPIDYVR, the launcher's default and game_xr's start value are OFF)
    imageStyle,
    // 0.0.73 DD BATON: ON (the held baton seated in the fist, the grapple fired from its silver tip, the tip flying
    // to the anchor on a rope and reeled back) or OFF (test 3's held baton and grapple line)
    ddBatonTip,
    // 0.0.73 STREAMER MODE (streamer_view.hpp): the monitor shows the headset's eye, every drawing on it (ON) or not (OFF);
    // STREAMER SMOOTH: how the monitor's view steadies head shake (LOW, OFF, HIGH); STREAMER EYE: LEFT EYE or RIGHT EYE
    streamerMode, streamerSmooth, streamerEye,
    // 0.0.73 HERO HANDS (suit_wear::HandProfile): IRON MAN's own hands - each MARK's gauntlet turned so its
    // repulsor is the real palm, the raised hand drawn palm out along the blast (ON) or test 4's shared hands (OFF)
    ironHands,
    // 0.0.73 GRAPPLE AIM (dd_baton073::aimTurn): DAREDEVIL's held baton turned silver tip ahead along the aim while
    // the grapple aims, flies and reels (ON) or kept in the fighting grip (OFF)
    ddGrappleAim,
    // 0.0.73 CLAW LOOK (hero007 appendClawSteel, suit_wear wolverine_* profiles): WOLVERINE's claws out between the knuckles along the fist, fanned,
    // bright steel (ON) or test 4's (OFF). 0.0.73 test 5 CLAW BONE: ON with a Wolverine suit, out of the suit's own
    // drawn fist, rooted inside it and locked to its hand bone (hero007::appendClawBone, suit_wear::clawPlace)
    wolverineClaws,
    // 0.0.73 LIGHT CHECKS (eye_queries.hpp): ON (each eye's light-glow visibility checks get the game view's
    // room, at least 1024, not the 256 the game gives secondary views) or OFF (the game's 256); next VR start
    lightChecks,
    // 0.0.73 GLOW CHECK (eye_queries.hpp glow_check): a live debug switch for the lights' halo / streak sprites:
    // GAME (the game's visibility test), ALWAYS (drawn untested) or HIDDEN (not drawn)
    glowCheck,
    // 0.0.73 SL WATCH (sl_watch.hpp): ON (default for the test build) logs what the game hands Streamline
    // (jitter, motion-vector scale and flags, near/far/FOV, frame token, viewport id) every 60 frames; OFF stops
    slWatch,
    // 0.0.73 MP SOLO TEST: a loopback ghost of you (or the MP Test Buddy) as PLAYER 2 over the UDP net code (ON) or not (OFF);
    // 0.0.74 test 1: back for the test build, OFF first (the default, value 0), ON to try it
    mpSoloTest,
    // 0.0.73 AA SHARPEN MATCH (eye_sharpen.hpp matchedPercent): ON (the default) sharpens by the share of
    // the eye history kept, none while a swing cuts it; OFF sharpens as set (test 5).
    sharpenMatch,
    // 0.0.73 test 6 CLAW LEVEL (suit_wear.hpp clawLevelDeg): ON (the default) the claws level with the hand (along
    // the hand bone, the same roots); OFF test 5's 8-30 degree tilt toward the back of the hand
    clawLevel,
    // 0.0.73 test 6 WINDOW POP FIX (window_pop.hpp): ON (the default) the game's widened view keeps flat play's
    // building detail distance (at most 2x); OFF test 5's
    windowPop,
    // 0.0.73 MPSUIT PLAYER 2 BODY (mp_body.hpp; the owner: "when im spiderman with ingame suits the other player
    // was that stick figure thing again"): SUIT LOOK (the default) draws PLAYER 2 without a mod suit model as a
    // shaded hero body (Spider-Man in the suit's learned colours); FIGURE test 6's stick figure
    mpPlayer2Body,
    // 0.0.73 COMING SOON (holo_roster.hpp soonHeroes): the hero tiles' SOON tab - the heroes coming in 0.0.74,
    // their pictures, not playable - shown (ON, the default) or hidden (OFF); live
    comingSoon,
    // 0.0.74 test 1 HIDEOUTS (hideout074.hpp): the van dash HOME page's GO TO SAFEHOUSE / GO TO BATCAVE (ON, the
    // default) or not; the rooms load at start only when ON (turning it on needs a restart)
    hideouts,
    // 0.0.74 test 1 CAR MUSIC (vehicle074.hpp): the van dash MUSIC page's player - IN VEHICLES (the default), ALWAYS
    // (on outside the van too), OFF; live
    carMusic,
    // 0.0.74 test 1 SEATED DRIVING (vehicle074.hpp): the dash's SIT button (SIT BUTTON, the default), AUTO SIT (sat a
    // second after getting in), OFF; getting out stands him back up; live. 0.0.74 test 10: AUTO SIT (0, the default:
    // sat and centred on the seat .3 s after the boarding), SIT BUTTON (1), OFF (2), HEIGHT ONLY (3: test 1's SIT)
    seatedDriving,
    // 0.0.74 test 1 BATMOBILE (batmobile074.hpp; owner pt1159u "add the Batmobile's to test 1"): ON (the default)
    // Batman (every suit) summons a drivable Batmobile on X, the van's driving; its models load at start only when ON
    // (turning it on needs a restart). Anything missing: no car, a note, never a crash
    batmobile,
    // 0.0.74 test 1 BATMOBILE CHOICE: 1989 (the default, Dreamloft3D), ODAMI, FOREVER (SorroW), FOREVER 2 (S.York);
    // the dash HOME page's BATMOBILE button steps it too; live (the next summon)
    batmobileChoice,
    // 0.0.74 test 2 CABIN CLEAR: ON (the default) or OFF
    cabinClear,
    // 0.0.74 test 2 CAR PAINT: SATIN (the default) or FLAT
    carPaint,
    // 0.0.74 test 2 SEATED TOOLS: BLOCKED (the default) or ALLOWED
    seatedTools,
    // 0.0.74 test 2 DASH POINTER: ON (the default) or OFF
    dashPointer,
    // 0.0.74 test 2b CRIME GPS: ON (the default) or OFF - crime popups, MAP and HUD arrow in the Batmobile and van
    crimeGps,
    // 0.0.74 test 3 WHEEL FIRST (van072.hpp)
    wheelFirst,
    // 0.0.74 test 3 CAVE QUIET (cave_quiet074.hpp)
    caveQuiet,
    // 0.0.74 test 3 FINGER RAY (finger_ray074.hpp)
    fingerRay,
    // 0.0.74 test 3 DASH INSIDE (cabin_look074.hpp)
    dashInside,
    // 0.0.74 test 3 COCKPIT GLOW (cabin_look074.hpp)
    cockpitGlow,
    // 0.0.74 test 10 QUICK HOOK (hero072.hpp HookLauncher::quick): ON (0, the default) the PUNISHER's hook fires on
    // the trigger press (no windup, no rearm) and its ask is held while the trigger is; OFF test 9's 0.25 s windup
    punisherQuickHook,
    // 0.0.74 test 10 GRAPPLE YANK (CANDIDATE, grapple074.hpp): ON (0, the default) a grapple that reels onto an enemy
    // also sends the game a knockback from him toward the hero (DirectDamage, 1 HP); the hero's reel stays the fallback
    grappleYank,
    // 0.0.74 test 10 SURE GRAPPLE (grapple074::AskLatch): ON (0, the default) every hero's grapple press is held
    // until the bridge answers it (a grapple or a miss) or 150 ms pass; OFF test 9's one-frame ask
    sureGrapple,
    // 0.0.74 test 10 BELT ZONES (hero007.hpp pickZone): ON (0, the default) BATMAN's grip draws the grapple gun
    // only at the left hip, a batarang only at the belt's front centre, a smoke pellet only from the other
    // forearm (the nearest zone, by a margin); OFF test 9's buckle smoke / side pouches / any other grip's gun
    beltZones,
    // 0.0.74 test 10 ENTRY FACING (van072.hpp Tune::boardFaceRoom): PLAY SPACE (0, the default) or HEAD (1)
    entryFacing,
    // 0.0.74 test 10 HIDEOUT HOLD (van_screen.hpp): HOLD (0, the default: GO TO BATCAVE / SAFEHOUSE held 1 s) or TAP
    hideoutHold,
    // 0.0.74 test 10 THROW BY SWING (throw_swing.hpp): ON (0, the default) every hand throw flies along the arm's
    // swing (the peak window's mean velocity); OFF (1) the old line (the hand's aim at the release)
    throwSwing,
    // 0.0.74 test 10 GLIDE TILT (cape_glide.hpp handTilt / tiltPitch): ON (0, the default) BATMAN's glide pitch is
    // the controllers' tilt, directly (Shadow's way); OFF (1) the arm poses' pitch (out glides, tuck dives, raise pulls up)
    glideTilt,
    options
};
struct OptionInfo {
    const char* key;       // tools/live_settings.py's name
    const char* label;     // the hologram's row
    const char* shortName; // the hologram's narrow column
    const char* help;      // the one sentence under it
    uint32_t count;
    const char* values[5];
};
inline constexpr OptionInfo optionInfo[options] = {
    {"crawl_cap", "CRAWL CAP", "CRAWL", "Crouches the view only under a low ceiling, never in a fight, no restart.", 3,
     {"AUTO", "WITH FLAG", "OFF", nullptr}},
    {"zip_mode", "ZIP MODE", "ZIP",
     "SPIDYVR lunges at the foe you look at as you punch, no restart.", 4,
     {"SPIDYVR", "BOTH", "DISTANCE", "AIMED"}},
    {"aim_assist", "BALL AIM", "AIM", "SMART aims at the enemy nearest your aim, at his chest, no restart.",
     4, {"SMART", "SPIDYVR", "CONE 20", "OFF"}},
    {"ball_look", "BALL LOOK", "BALL",
     "OURS fires one mod ball from your wrist; the others the game's, no restart.", 4,
     {"OURS", "WRIST", "GLOB", "GAME ONLY"}},
    {"punch_mode", "PUNCH MODE", "PUNCH", "TARGETED attacks only an enemy in reach or locked, no restart.", 4,
     {"TARGETED", "BOTH", "GESTURE", "CONTACT"}},
    {"ring_anchor", "RING ANCHOR", "RING", "Pins the health ring to the drawn wrist's web shooter, no restart.",
     3, {"WRIST", "HAND", "WORLD", nullptr}},
    {"holo_open", "HOLO OPEN", "HOLO",
     "Point at your forearm and pull the trigger to open it, no restart.", 4,
     {"FOREARM", "PALM LOOK", "CLICK", "F8 ONLY"}},
    {"wall_tilt", "WALL TILT", "TILT", "Levels the view on death, respawn and off a steep wall, no restart.", 2,
     {"SAFE", "OFF", nullptr, nullptr}},
    {"web_rope", "WEB ROPE", "ROPE", "Draws the swing web as a thick bending rope or the frayed fibre, no restart.", 2,
     {"ROPE", "FIBRE", nullptr, nullptr}},
    {"lunge", "LUNGE", "LUNGE", "GAME BUTTON uses the game's web strike; SPIDYVR MOVE flies you, no restart.", 2,
     {"GAME BUTTON", "SPIDYVR MOVE", nullptr, nullptr}},
    {"ball_fire", "BALL FIRE", "FIRE", "Shoots the web ball as the trigger goes down, or as it is let go, no restart.", 2,
     {"PRESS", "RELEASE", nullptr, nullptr}},
    {"aim_axis", "AIM AXIS", "AXIS", "Aims balls, punches and blood along your hand or an older axis, no restart.", 3,
     {"HAND -Y", "AIM POSE", "OLD -Z", nullptr}},
    {"ring_layer", "RING LAYER", "LAYER", "HOLD never hides the ring; NEWEST hides it in a stale picture, no restart.",
     3, {"HOLD", "NEWEST", "IMAGE", nullptr}},
    {"lunge_from", "LUNGE FROM", "FROM", "Only swings at an enemy at least this far away lunge, no restart.", 3,
     {"2.5 M", "1.3 M", "4 M", nullptr}},
    {"lunge_speed", "LUNGE SPEED", "SPEED", "How fast a swing must be, in metres a second, to lunge, no restart.", 3,
     {"2.2", "2.6", "1.9", nullptr}},
    {"lunge_aim", "LUNGE AIM", "LAIM", "STEADY lunges only at a visible enemy you look at, no restart.", 2,
     {"STEADY", "38", nullptr, nullptr}},
    {"punch_angle", "PUNCH ANGLE", "ANGLE", "How straight ahead a fast swing must go to count as a punch, no restart.", 3,
     {"WIDE .2", "38 .35", "ANY 0", nullptr}},
    {"ball_lead", "BALL LEAD", "LEAD", "ON TARGET: a ball's RB goes at once; ALWAYS: 30 ms late, no restart.", 2,
     {"ON TARGET", "ALWAYS", nullptr, nullptr}},
    {"blood_look", "BLOOD LOOK", "BLOOD", "BRUTAL: big dark lasting drops, a floor pool and glove blood, no restart.",
     3, {"BRUTAL", "BIG", "37 SMALL", nullptr}},
    {"melee_anim", "MELEE ANIM", "MELEE", "EVERY HIT: one X per punch at once; SPACED keeps old gaps, no restart.", 3,
     {"EVERY HIT", "GAME", "SPACED", nullptr}},
    {"gadget_keys", "GADGET KEYS", "KEYS", "Which RB button each gadget sends: the mod's, the game's or yours, no restart.",
     3, {"38.2", "GAME DEFAULT", "CUSTOM", nullptr}},
    {"grab_web", "GRAB WEB", "GRAB", "Grip toward an enemy to web him, pull back to yank him, no restart.", 3,
     {"ASSIST", "EXACT", "OFF", nullptr}},
    {"mirror_look", "MIRROR LOOK", "MIRROR", "SUIT: your worn suit on your pose; FIGURE is the older look, no restart.", 2,
     {"SUIT", "FIGURE", nullptr, nullptr}},
    {"gadget_feel", "GADGET FEEL", "FEEL", "EASY: simpler, looser gadget moves with a buzz, no restart.", 2,
     {"EASY", "38.1", nullptr, nullptr}},
    // Update 38.3 (run382/DIAGNOSIS.md)
    {"look_lock", "LOOK LOCK", "LOOK", "QUICK moves the lock to the enemy you look straight at, no restart.", 2,
     {"QUICK", "STEADY", nullptr, nullptr}},
    {"strike_steer", "STRIKE STEER", "STEER", "TO LOCK points the stick at your locked enemy for a strike, no restart.", 2,
     {"TO LOCK", "OFF", nullptr, nullptr}},
    {"far_windows", "FAR WINDOWS", "WINDOWS", "Eye building detail farther so night windows light sooner (GPU), no restart.", 4,
     {"2X", "3X", "1.5X", "GAME"}},
    {"gadget_hint", "GADGET HINT", "HINT", "Names each gadget at your wrist as it fires, no restart.", 2,
     {"ON", "OFF", nullptr, nullptr}},
    // Version 0.0.2 (Update 38.6)
    {"aim_strength", "AIM STRENGTH", "STRENGTH", "How hard an OURS ball bends to an enemy: cone, homing, lead, no restart.", 5,
     {"STRONG", "MAX", "OFF", "LOW", "NORMAL"}},
    {"ball_hit", "BALL HIT", "HIT", "+GAME WEB also asks the game for its own web ball at a hit, no restart.", 2,
     {"OURS ONLY", "+GAME WEB", nullptr, nullptr}},
    // Version 0.0.3 (Update 38.7): rope_grab.hpp, wall_stick.hpp, auto_perf.hpp
    {"rope_grab", "ROPE GRAB", "TETHER", "ON: grip to rope an enemy 3-9 m away, fling or pull to yank, no restart.", 2,
     {"OFF", "ON", nullptr, nullptr}},
    {"rope_strength", "ROPE STRENGTH", "PULL", "How far the held rope's end moves per metre your hand moves, no restart.", 3,
     {"NORMAL", "LOW", "STRONG", nullptr}},
    {"wall_stick", "WALL STICK", "STICK", "Nets an enemy you throw or punch into a wall or floor, no restart.", 3,
     {"OFF", "LOW HEALTH ONLY", "ALWAYS", nullptr}},
    {"auto_perf", "AUTO PERFORMANCE", "AUTOPERF", "Lowers graphics when frames drop and raises them again, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    {"perf_target", "TARGET FPS", "TARGET", "The frame rate it aims for; AUTO follows your headset, no restart.", 5,
     {"AUTO", "72", "80", "90", "120"}},
    {"perf_floor", "QUALITY FLOOR", "FLOOR", "How far it may lower graphics: HIGH a little, LOW the most, no restart.", 3,
     {"MEDIUM", "HIGH", "LOW", nullptr}},
    {"perf_status", "SHOW STATUS", "STATUS", "Shows its frame rate and current step at your left wrist, no restart.", 2,
     {"ON", "OFF", nullptr, nullptr}},
    // Version 0.0.4 (Update 38.8) IRON MAN ABILITIES (hero_modes.hpp; option shooterType, value 1 the
    // repulsor): OFF (0, Spider-Man) is the default; ON fires no web of any kind and sends none to the game.
    {"iron_man", "IRON MAN ABILITIES", "IRONMAN", "ON: blasts and a chest beam that hurt enemies, no webs, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    // 0.0.72 MIRROR VIEW (EXPERIMENTAL, merged from the private mirror test): OFF by default; ON (live.txt
    // "test") makes the view at the next start; it turns itself OFF on any problem (mirror_view::finished).
    {"mirror_view", "MIRROR VIEW (EXPERIMENTAL)", "MIRRORCAM", "EXPERIMENTAL: the game's own Spider-Man image, not mod suits; needs a restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    // Version 0.0.5 (Update 38.9) HERO HEIGHT: the VR body k times the player (a Hulk skin stands
    // straight); live; 1.00X (0) the default, nothing changed.
    {"hero_height", "HERO HEIGHT", "HEIGHT", "Grows your VR body for a big skin like Hulk so its legs stand straight, live.",
     5, {"1.00X", "1.15X", "1.30X", "1.45X", "1.60X"}},
    // WEB BALL DAMAGE (0.0.5's DAMAGE TEST; sm2_damage.hpp), OFF by default. Version 0.0.6: the heroes'
    // own hits always send their blows (rep0050 CONFIRMED the path); this is Spider-Man's web ball only.
    {"hero_damage", "WEB BALL DAMAGE", "DAMAGE", "ON: Spider-Man's web ball hits hurt enemies too, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    // Version 0.0.5 PUNCH SENSITIVITY (punchFeelOf): FORGIVING (0, the default) is easier than 0.0.4.
    {"punch_sense", "PUNCH SENSITIVITY", "SENSE", "How easily a fist counts as a punch; FORGIVING takes sloppy jabs, no restart.",
     4, {"FORGIVING", "MAX", "0.0.4", "STRICT", nullptr}},
    // SFX VOLUME (sfx.hpp): the mod's own sounds (repulsor, beam, batons, sonic boom).
    {"sfx_volume", "SFX VOLUME", "SFX", "How loud the mod's own sounds are, no restart.", 5,
     {"100%", "75%", "50%", "25%", "OFF"}},
    // SUPERSONIC (fly_boost.hpp): in FLY MODE, the left stick forward while flying hands-out (ON by default).
    {"supersonic", "SUPERSONIC", "SONIC", "Both hands out in FLY MODE, left stick forward for a sonic boom, no restart.", 2,
     {"ON", "OFF", nullptr, nullptr}},
    {"supersonic_speed", "SUPERSONIC SPEED", "MACH", "How many times your FAST fly speed the sonic dash reaches, no restart.", 4,
     {"2X", "1.5X", "2.5X", "3X", nullptr}},
    // Version 0.0.6 (Update 39.0) DAREDEVIL / HULK ABILITIES: OFF by default (experimental); turning one
    // on turns the other heroes off (heroOnly). DAREDEVIL PUNCH: 3X (0) the default, live.
    {"daredevil", "DAREDEVIL ABILITIES", "DAREDEVIL", "ON: thrown batons, harder punches and sonar sight, no webs, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    {"hulk", "HULK ABILITIES", "HULK", "ON: a 4x charged jump and one-punch knockouts, no webs, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    {"daredevil_punch", "DAREDEVIL PUNCH", "DDPUNCH", "How many normal punches each DAREDEVIL punch hits like, live.",
     5, {"3X", "1X", "2X", "4X", "5X"}},
    // Version 0.0.62 HIT INDICATOR: OFF (0) the default; a brief X where your hits land, live.
    {"hit_indicator", "HIT INDICATOR", "HITMARK", "ON: a brief white X flashes where your hits land, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    // HAND FLIPS: ON (0) keeps 0.0.6 (hand swings add to REALISTIC FLIPS); OFF: no flip from moving
    // your hands, live.
    {"hand_flips", "HAND FLIPS", "HANDFLIP", "OFF: swinging your arms never starts a flip, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    // LENS FLARES: GAME (0) leaves the game's setting alone; OFF IN VR turns the game's lens flares
    // off while VR runs (they test the monitor's view, not each eye's, and shine through walls),
    // RESTART REQUIRED, put back when VR closes. 0.0.73 lights: the launcher starts it OFF IN VR (test 4's
    // video: flare streaks strobing, headlights ghosting); index 0 stays GAME so live.txt numbers and the
    // wrist readback bit keep their meaning (the launcher's default, not index 0, is the default here).
    {"lens_flares", "LENS FLARES", "FLARES", "OFF IN VR turns the game's lens flares off in VR, restart VR to apply.",
     2, {"GAME", "OFF IN VR", nullptr, nullptr}},
    // Version 0.0.62 PUNCH HITS (SpidyVR's land(): the hit is sent the moment the fist touches):
    // GAME + INSTANT (0, the default) sends a small CONFIRMED DirectDamage blow with a knock on the
    // contact frame and still presses the game's X for its move; GAME MOVE (1) is 0.0.6 (the X
    // only: the hit lands when the game's animation does); INSTANT ONLY (2) sends no X (no game
    // animation pulls you; the X comes back by itself if the damage path is closed). Live.
    {"punch_hits", "PUNCH HITS", "HITS", "GAME + INSTANT hurts foes as your fist lands; GAME MOVE waits, no restart.",
     3, {"GAME + INSTANT", "GAME MOVE", "INSTANT ONLY", nullptr}},
    // ENEMY TRACKING: LIVE (0, the default) places enemies by their own world positions, re-read 30
    // times a second; 0.0.6 (1) by their offset from you, re-read 10 times a second (while you move,
    // lunge or swing they trail behind where they are). Live.
    {"foe_track", "ENEMY TRACKING", "FOES", "LIVE: lock-on and punches follow enemies 30 times a second, no restart.",
     2, {"LIVE", "0.0.6", nullptr, nullptr}},
    // BEACONS IN VISION: ON (0, the default) keeps the game's bright coloured beacons, mission icons and
    // markers drawn into the world in their own colours, outlined and gently pulsing, on top of
    // DAREDEVIL's sonar look (eye_color::sonarPixel); OFF (1) is 0.0.6's sonar. Live.
    {"beacons_vision", "BEACONS IN VISION", "BEACONS", "ON: mission markers stay bright in Daredevil's sonar, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    // Version 0.0.7 HERO PROFILES: OFF by default; turning one on turns every other hero off (heroOnly).
    {"batman", "BATMAN ABILITIES", "BATMAN", "ON: batarangs, grapple, glide and smoke pellets, no webs, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    {"wolverine", "WOLVERINE ABILITIES", "WOLVERINE", "ON: claws by grip fist and fast healing, no webs, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    {"superman", "SUPERMAN ABILITIES", "SUPERMAN", "ON: heat vision, cold breath and super strength, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    {"punisher", "PUNISHER ABILITIES", "PUNISHER", "ON: two hip pistols and a sawed-off on your back, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    // BATARANG CHAIN: 25% (0, the default) - each hit's own roll to bounce on (3 enemies at most).
    {"batarang_chain", "BATARANG CHAIN", "CHAIN", "The chance each batarang hit bounces on to another enemy, live.",
     5, {"25%", "50%", "75%", "100%", "OFF"}},
    // EASY RELOAD: AUTO (0, the default) or REALISTIC (a downward flick drops the mag, the hip reloads).
    {"easy_reload", "EASY RELOAD", "RELOAD", "AUTO refills an empty gun; REALISTIC drops the mag, no restart.",
     2, {"AUTO", "REALISTIC", nullptr, nullptr}},
    // HOLSTER HANDS: SAME SIDE (0, the default) or EITHER HAND (a cross draw); the back is either hand's.
    {"holster_hands", "HOLSTER HANDS", "HOLSTERS", "SAME SIDE: each hand draws only from its own hip, live.",
     2, {"SAME SIDE", "EITHER HAND", nullptr, nullptr}},
    // BATON HOLSTERS: Version 0.0.72 (the user: no magic spawning) always on - DAREDEVIL's batons
    // ride on his back and a grip over a shoulder draws one; the trigger never spawns a baton.
    // The value is kept for old live.txt files; both values are ON.
    {"baton_holsters", "BATON HOLSTERS", "BHOLSTER", "Always on now: grip over a shoulder to draw, no restart needed.",
     2, {"ON", "ALWAYS ON", nullptr, nullptr}},
    // BATON LOOK: CLASSIC (0, the default: the baton as in 0.0.62/0.0.63), CLUB (the 0.0.7 billy club) or MODEL (Anthony Yanez's
    // "Daredevil's baton", rope removed, split in two: Models\Baton\Left|Right; CLUB when missing).
    {"baton_look", "BATON LOOK", "BLOOK", "CLASSIC: the red bar; CLUB: a billy club; MODEL: real batons, no restart.",
     3, {"CLASSIC", "CLUB", "MODEL", nullptr}},
    // BATARANG STYLE: DEFAULT (0, the default: the built-in batarang) or one of the three models the
    // player chose on Sketchfab, read from <Spidy folder>\Models\Batarang\<STYLE>\ (mesh_model.hpp);
    // a missing or broken file draws DEFAULT. Live.
    {"batarang_style", "BATARANG STYLE", "BSTYLE", "DEFAULT: the built-in batarang; the others are models, no restart.",
     4, {"DEFAULT", "EIRA", "CLOONEY", "JOKER"}},
    // GUN MODELS: BUILT-IN (0, the default) or MODELS (Models\Pistol, Models\SawedOff; built-in when missing).
    {"gun_models", "GUN MODELS", "GMODELS", "MODELS: Punisher's guns as the converted 3D models, no restart.",
     2, {"BUILT-IN", "MODELS", nullptr, nullptr}},
    // Version 0.0.7 CAPTAIN AMERICA: OFF by default (a hero toggle: one hero at a time).
    {"captain_america", "CAPTAIN AMERICA ABILITIES", "CAPTAIN", "ON: his shield on your back to draw, guard and throw, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    // SHIELD ARM: RIGHT (0, the default) or LEFT (1); only that hand draws it (captain::armHand).
    {"shield_arm", "SHIELD ARM", "SARM", "RIGHT or LEFT: the forearm that wears and draws the shield, live.",
     2, {"RIGHT", "LEFT", nullptr, nullptr}},
    // SHIELD DAMAGE: 35 HP (0, the default) a thrown hit, on sm2_damage's CONFIRMED path.
    {"shield_damage", "SHIELD DAMAGE", "SDAMAGE", "How much each thrown shield hit takes off an enemy, live.",
     4, {"35 HP", "20 HP", "50 HP", "70 HP"}},
    // SHIELD CHAIN: ON (0, the default: ricochets like the batons, 3 enemies at most) or OFF (one, home).
    {"shield_chain", "SHIELD CHAIN", "SCHAIN", "ON: a thrown shield ricochets to two more enemies, then back, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    // SHIELD BLOCK: OFF (0, the default) or EXPERIMENTAL (CANDIDATE: a hit while the shield faces the
    // nearest enemy is given back through the CONFIRMED health record; never above the cap, never a revive).
    {"shield_block", "SHIELD BLOCK", "SBLOCK", "EXPERIMENTAL (CANDIDATE): a hit at the shield's face is refunded, no restart.",
     2, {"OFF", "EXPERIMENTAL", nullptr, nullptr}},
    // SHIELD LOOK: BUILT-IN (0, the default: drawn rings and star) or MODEL (Models\Shield; BUILT-IN when missing).
    {"shield_look", "SHIELD LOOK", "SLOOK", "MODEL: the converted 3D shield, else BUILT-IN rings, no restart.",
     2, {"BUILT-IN", "MODEL", nullptr, nullptr}},    // Version 0.0.72 (TEST): THOR (hero072.hpp) - one hero at a time like the others.
    {"thor", "THOR ABILITIES", "THOR", "ON: play as Thor with his hammer on your back, live (no restart).",
     2, {"OFF", "ON", nullptr, nullptr}},
    {"thor_lightning", "THOR LIGHTNING", "TLIGHT", "ON: lightning crawls on Thor's arms and hammer in a fight, live (no restart).",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"thor_storm", "THOR STORM", "TSTORM", "ON: hold the hammer straight up overhead 1 s to strike enemies near, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"thor_chain", "HAMMER CHAIN", "TCHAIN", "ON: a thrown hammer jumps to two more enemies then flies back, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    // Version 0.0.72 (TEST): GREEN LANTERN and his six powers.
    {"green_lantern", "GREEN LANTERN ABILITIES", "LANTERN", "ON: play as Green Lantern with ring constructs, no webs, live (no restart).",
     2, {"OFF", "ON", nullptr, nullptr}},
    {"gl_flight", "RING FLIGHT", "GLFLY", "ON: raise both arms straight up to take off, hands steer, live (no restart).",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"gl_fist", "CONSTRUCT FIST", "GLFIST", "ON: hold the right grip and punch fast for a giant green fist, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"gl_beam", "RING BEAM", "GLBEAM", "ON: the right trigger fires the ring's beam, live (no restart).",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"gl_bubble", "SHIELD BUBBLE", "GLBUBL", "ON: hold both grips and pull your hands apart for a bubble, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"gl_chain", "CHAIN CONSTRUCT", "GLCHAN", "ON: the left trigger throws a green chain that yanks an enemy, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"gl_charge", "RING CHARGE", "GLCHRG", "ON: the left wrist's ring charge drains with powers, not flight, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    // Version 0.0.72 (TEST): the list's rules.
    {"spider_climb_only", "SPIDER CLIMB ONLY", "SPCLMB", "ON: only Spider-Man crawls on walls, the other heroes cannot, live (no restart).",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"uppercut", "UPPERCUT LAUNCH", "UPPER", "ON: a fast upward punch that lands launches the enemy, live (no restart).",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"smoke_blind", "SMOKE BLIND", "SBLIND", "ON: enemies in Batman's smoke stagger blind, live (no restart).",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"dd_grapple", "BATON GRAPPLE", "DDGRAP", "ON: grip draws the baton, trigger grapples, a throw lets it fly, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"dd_parry", "BATON PARRY", "DDPARY", "ON (CANDIDATE): batons crossed in front of your face block hits, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"iron_hud", "IRON MAN HUD", "IRNHUD", "ON: Iron Man's helmet HUD with lock box, hostiles, speed, altitude, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"punisher_rage", "PUNISHER RAGE", "RAGE", "ON: enough damage or downs in a fight turn the Punisher's view red, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"reputation", "HERO REPUTATION", "REP", "ON: each hero's rank, up for crimes stopped, down for ones left, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"survival", "SURVIVAL MODE", "SURVIV", "ON: hits can hurt a limb that heals over time (not every hero), no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    {"punisher_suit", "PUNISHER SUIT", "PSUIT", "Which Punisher suit kit you installed, VEST or WARZONE, no restart.",
     2, {"VEST", "WARZONE", nullptr, nullptr}},
    // Version 0.0.72 (the user, 8:23): THOR and SUPERMAN are invincible by nature (INVINCIBLE's
    // CONFIRMED health refill while either is played; never a revive, so scripted deaths still happen).
    {"natural_invincible", "NATURAL INVINCIBLE", "NATINV", "ON: Thor and Superman take no damage or injuries, live (no restart).",
     2, {"ON", "OFF", nullptr, nullptr}},
    // Version 0.0.72 (the user, 8:24): HULK RAGE, damage taken in a fight builds his punch multiplier.
    {"hulk_rage", "HULK RAGE", "HRAGE", "ON: Hulk hits harder the more he is hurt in a fight, up to 2x, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"rage_blood", "RAGE BLOOD", "RBLOOD", "EXTREME: enemies shot in Punisher rage bleed a lot, no restart.",
     3, {"EXTREME", "NORMAL", "OFF", nullptr}},
    {"rage_tint", "RAGE TINT", "RTINT", "How strong the Punisher's red heartbeat rage vision is, no restart.",
     4, {"NORMAL", "STRONG", "SUBTLE", "OFF"}},
    {"fire_douse", "FIRE DOUSE", "DOUSE", "ON: each hero has a way to put out fires (hook CANDIDATE), no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"shield_slam", "SHIELD SLAM", "SSLAM", "ON: slam Cap's shield down to knock enemies back and douse fire, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"punisher_hook", "GRAPPLE HOOK", "PHOOK", "ON: grip at the chest draws the hook, trigger fires it, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    // Version 0.0.73 (fixes 11 + 18): OFF by default - only SPIDER-MAN climbs walls unless this is turned on.
    {"claw_climb", "CLAW CLIMB", "CLAWUP", "OFF by default; ON lets Wolverine climb walls with claws out, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    {"npc_shove", "SHOVE", "SHOVE", "ON: push both open hands fast at a close enemy to shove him back, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"faction", "ENEMY FACTION", "FACTN", "AUTO matches the enemies to the hero (hook CANDIDATE), no restart.",
     5, {"AUTO", "VANILLA", "THE HAND", "HYDRA", "JOKER GANG"}},
    {"hero_suit", "HERO SUIT", "HSUIT", "Steps through this hero's unlocked suits (each artist credited), no restart.",
     2, {"DEFAULT", "NEXT", nullptr, nullptr}},
    {"test_unlocks", "TEST UNLOCKS", "UNLOCK", "TESTING: SUITS, ULTIMATES (+ suits) or ALL (+ perks) open now, no restart.",
     4, {"OFF", "SUITS", "ULTIMATES", "ALL"}},
    {"ultimates", "ULTIMATES", "ULTIMT", "ON: max reputation's ultimate, both fists crossed on the chest, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"perk_branch", "HERO PERKS", "PERKS", "How perk points are spent: in order, chain first, held or off (no restart).",
     4, {"AUTO", "CHAIN FIRST", "HOLD POINTS", "PERKS OFF"}},
    {"own_suit", "USE MY OWN OR GAME SUIT", "OWNSUIT", "Fallback for a model glitch or crash: use your mod's or game's suit, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    {"perch_takedown", "PERCH TAKEDOWN", "PERCH", "ON: Batman quick-presses the game's prompt button to hang a foe, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"takedown_light", "TAKEDOWN LIGHT", "TLIGHT", "GREEN: no one sees the takedown; RED: others get alerted, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"punisher_van", "PUNISHER VAN (EXPERIMENTAL)", "VAN", "ON by default: X calls Punisher's van, parked if it cannot drive, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"thor_hammer", "THOR HAMMER", "HAMMER", "Picks which Mjolnir model Thor draws from his back, no restart.",
     4, {"BOOOOP", "DEVILSEYE", "LOVE THUNDER", "FCCOOPER"}},
    {"game_body", "GAME BODY (EXPERIMENTAL)", "GBODY", "EXPERIMENTAL: game's own body only after every safety check passes, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    {"thor_hammer_model", "THOR HAMMER MODEL", "HMODEL", "ON: Thor draws the THOR HAMMER model; OFF: no hammer model drawn, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"thor_flight", "THOR FLIGHT", "TFLY", "ON: Thor flies with both arms up; OFF: he stays on the ground, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"suit_body", "SUIT BODY", "SBODY", "Your picked suit model on your arms or body, live; the mirror shows it whole.",
     3, {"ARMS", "FULL BODY", "OFF", nullptr}},
    {"cap_sprint", "CAP SPRINT", "SPRINT", "Cap pumps his arms, left stick pushed, to super-sprint on foot, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"cull_margin", "CULL MARGIN", "CULL", "Wider stops far window and street light flicker, costs a little FPS, no restart.",
     4, {"10 DEG", "6 DEG", "15 DEG", "20 DEG"}},
    {"shadow_match", "SHADOW MATCH", "SMATCH", "Both eyes' shadows fit one lens for less shimmer, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"calib_look", "CALIBRATION LOOK", "CALIB", "HOLO adds belt, back and hip touches that fit your holsters, no restart.",
     2, {"HOLO", "CLASSIC", nullptr, nullptr}},
    {"fly_pose", "FLIGHT POSE", "FPOSE", "LEAN tips the body with flight speed, upright to hover, no restart.",
     2, {"LEAN", "UPRIGHT", nullptr, nullptr}},
    {"weapon_collision", "WEAPON COLLISION", "WCOLL", "Thrown and held weapons meet walls, cars and the ground, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"light_hold", "LIGHT HOLD", "LHOLD", "Steadies flickering street lights, STRONG steadies more, no restart.",
     3, {"ON", "STRONG", "OFF", nullptr}},
    {"van_screen", "VAN SCREEN", "VSCRN", "Shows a Punisher dash screen in the van - X or a poke pages it, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"palm_blast", "PALM BLAST", "PALMB", "PALM fires straight out of your open palm, AIM along the hand, no restart.",
     2, {"PALM", "AIM", nullptr, nullptr}},
    {"hero_menu", "HERO MENU", "HMENU", "HOLO TILES picks heroes on the hologram, CLASSIC the big popup, no restart.",
     2, {"HOLO TILES", "CLASSIC", nullptr, nullptr}},
    {"rep_log", "REP LOG", "REPLG", "ON logs each rep change on the hologram's REP tab, OFF stops it, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"van_pedals", "VAN PEDALS", "VPEDL", "WHEEL HAND: the wheel hand's trigger drives, the free hand fires, no restart.",
     2, {"WHEEL HAND", "CLASSIC", nullptr, nullptr}},
    {"image", "IMAGE", "IMAGE", "SPIDYVR: sharp, no smear; CLASSIC: the older look; textures change on restart.",
     2, {"SPIDYVR", "HERO VR CLASSIC", nullptr, nullptr}},
    {"image_style", "GAME TAA", "GTAA", "OFF keeps the picture steady; SPIDYVR runs the game's TAA per eye, no restart.",
     2, {"SPIDYVR", "OFF", nullptr, nullptr}},
    {"dd_baton_tip", "BATON TIP", "DDTIP", "ON: the baton sits in the fist, its silver tip flies on a rope, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"streamer_mode", "STREAMER MODE", "STRMR", "ON shows the headset's view with all its drawings on the monitor, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"streamer_smooth", "STREAMER SMOOTH", "STRSM", "Steadies head shake in the monitor's view, HIGH the most, no restart.",
     3, {"LOW", "OFF", "HIGH", nullptr}},
    {"streamer_eye", "STREAMER EYE", "STREY", "Which headset eye the monitor shows in STREAMER MODE, no restart.",
     2, {"LEFT EYE", "RIGHT EYE", nullptr, nullptr}},
    {"iron_hands", "HERO HANDS", "IMHND", "ON: Iron Man's own hands, palm out when raised to fire; no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"dd_grapple_aim", "GRAPPLE AIM", "DDAIM", "ON: the baton's silver tip turns to the aim while grappling; no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"wolverine_claws", "CLAW LOOK", "WVCLW", "ON: claws out of the fist between the knuckles, on the hand; no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"light_checks", "LIGHT CHECKS", "LCHK", "ON lets each eye check every light's glow so halos hold, restart VR to apply.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"glow_check", "GLOW CHECK", "GLOW", "Debug: ALWAYS draws every light glow untested, HIDDEN draws none, no restart.",
     3, {"GAME", "ALWAYS", "HIDDEN", nullptr}},
    {"sl_watch", "SL WATCH", "SLW", "Test: logs the game's DLSS camera data for the export, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"mp_solo_test", "MP SOLO TEST", "MPSOL", "ON shows you 3 m ahead as PLAYER 2 over the UDP loopback test, no restart.",
     2, {"OFF", "ON", nullptr, nullptr}},
    {"sharpen_match", "SHARPEN MATCH", "SHM", "ON sharpens only what the eye history smoothed, none while you move; no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"claw_level", "CLAW LEVEL", "CLVL", "ON points the claws level with the hand, OFF keeps the older tilt; no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"window_pop", "WINDOW POP FIX", "WPOP", "ON keeps flat play's building detail in VR, up to 2x (moderate GPU); no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"mp_player2_body", "PLAYER 2 BODY", "P2B", "SUIT LOOK draws PLAYER 2 in game suits as a hero body; no restart.",
     2, {"SUIT LOOK", "FIGURE", nullptr, nullptr}},
    {"coming_soon", "COMING SOON", "SOON", "ON shows the heroes coming next update on the roster's SOON tab, no restart.",
     2, {"ON", "OFF", nullptr, nullptr}},
    {"hideouts", "HIDEOUTS", "HIDE", "Safehouse trips from the van dash, Batcave from the Batmobile, needs a restart.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"car_music", "CAR MUSIC", "CMUS", "The van dash's MUSIC page plays your songs, no restart needed.", 3, {"IN VEHICLES", "ALWAYS", "OFF", nullptr}},
    {"seated_driving", "SEATED DRIVING", "SEAT", "AUTO SIT centres you in the driver's seat as you get in; no restart needed.", 4, {"AUTO SIT", "SIT BUTTON", "OFF", "HEIGHT ONLY"}},
    {"batmobile", "BATMOBILE", "BMOB", "Drives a Batmobile as Batman (X summons it), turning on needs a restart.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"batmobile_choice", "BATMOBILE CHOICE", "BCAR", "Which Batmobile Batman's X summons (also the dash's BATMOBILE), no restart.", 4, {"1989", "ODAMI", "FOREVER", "FOREVER 2"}},
    {"cabin_clear", "CABIN CLEAR", "CABN", "ON hides the Batmobile roof while you sit inside so you see out; no restart.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"car_paint", "CAR PAINT", "PAINT", "SATIN: satin black shine; FLAT: no shine; TEST 1: test 1 car look; no restart.", 3, {"SATIN", "FLAT", "TEST 1", nullptr}},
    {"seated_tools", "SEATED TOOLS", "STOOL", "BLOCKED: no hero tools in Batmobiles, no grapple in any car; no restart.", 2, {"BLOCKED", "ALLOWED", nullptr, nullptr}},
    {"dash_pointer", "DASH POINTER", "DPTR", "ON aims a ray at the car dash screens; trigger or finger presses; no restart.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"crime_gps", "CRIME GPS", "CGPS", "ON shows crimes near your car: popup, MAP, HUD arrow; no restart.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"wheel_first", "WHEEL FIRST", "WHL1", "ON: a hand on or near the wheel steers and never opens the door; live.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"cave_quiet", "CAVE QUIET", "CAVQ", "ON quiets the city and the car engine in the Batcave and safehouse; live.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"finger_ray", "FINGER RAY", "FRAY", "ON aims the dash laser from your index fingertip and snaps to buttons; live.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"dash_inside", "DASH INSIDE", "DSIN", "ON hides the cockpit from outside; the door handle glows only near a hand; live.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"cockpit_glow", "COCKPIT GLOW", "CGLO", "ON darkens the Batmobile cabin and softly backlights dials and switches; live.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"punisher_quick_hook", "QUICK HOOK", "QHOOK", "ON fires the Punisher hook as you pull the trigger, no lost press; live.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"grapple_yank", "GRAPPLE YANK", "GYNK", "ON tries to yank a hooked enemy to you (CANDIDATE); you still reel to him; live.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"sure_grapple", "SURE GRAPPLE", "SGRP", "ON holds each grapple press until the game takes it, every hero; live.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"belt_zones", "BELT ZONES", "BELTZ", "ON grapple at left hip, batarang at belt front, smoke on forearm; no restart.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"entry_facing", "ENTRY FACING", "EFAC", "PLAY SPACE turns your play area to the car's front on entry; no restart needed.", 2, {"PLAY SPACE", "HEAD", nullptr, nullptr}},
    {"hideout_hold", "HIDEOUT HOLD", "HHLD", "HOLD: GO TO BATCAVE / SAFEHOUSE on the dash needs a 1 s hold; no restart needed.", 2, {"HOLD", "TAP", nullptr, nullptr}},
    {"throw_swing", "THROW BY SWING", "TSWG", "ON throws fly the way your arm swung, OFF is the old aim; no restart.", 2, {"ON", "OFF", nullptr, nullptr}},
    {"glide_tilt", "GLIDE TILT", "GTLT", "ON: tilt your controllers to set the glide dive angle, like Shadow; no restart.", 2, {"ON", "OFF", nullptr, nullptr}},
};
// 0.0.73 IMAGE (the owner after test 3: "whatever spidyVR does to handle it works and it needs to look as
// good as that"): value 0 SPIDYVR (the default), 1 HERO VR CLASSIC (test 3's look). SpidyVR (upstream
// 0.6.0) runs the game's own TAA per eye with the game's sharpening at its lowest and no FXAA, CAS or mip
// bias of its own; HERO VR's engine TAA still shakes distant buildings (CANDIDATE: jitter units, per-eye
// motion vectors), so SPIDYVR here is the nearest we can run: EYE AA's history kept, read sharp
// (Catmull-Rom), with the smoothing (FXAA), the sharpening (140) and the softer textures (+0.5) taken off.
// Live but the textures (the mip bias is set when the game starts).
struct ImageLook {
    uint32_t aa;       // SpidyEyeAA: 0 off, 1 FXAA
    uint32_t sharpen;  // SpidyEyeSharpen, 0-200
    bool sharpHistory; // SpidyEyeHistorySharp
};
inline ImageLook imageLook(uint32_t value) {
    return value == 1 ? ImageLook{1, 140, false} : ImageLook{0, 0, true};
}
// 0.0.73 test 3 VAN PEDALS: value 0 WHEEL HAND (the default), 1 CLASSIC (0.0.72: either trigger is a pedal).
inline bool vanWheelHandPedals(uint32_t value) { return value == 0; }
// 0.0.73 test 3: SHADOW MATCH value 0 is ON (the default), 1 OFF (test 2 had them the other way round).
inline bool shadowMatchOn(uint32_t value) { return value == 0; }
// Version 0.0.62 HAND FLIPS / LENS FLARES values
inline bool handFlipsOn(uint32_t value) { return value != 1; }
inline bool lensFlaresOffInVr(uint32_t value) { return value == 1; }
// 0.0.74 test 2: CABIN CLEAR, DASH POINTER (0 ON, 1 OFF), CAR PAINT (0 SATIN, 1 FLAT), SEATED TOOLS
// (0 BLOCKED, 1 ALLOWED); anything unknown: the fix (value 0)'s.
inline bool cabinClearOn(uint32_t value) { return value != 1; }
inline bool satinPaintOn(uint32_t value) { return value != 1 && value != 2; }
// 0.0.74 test 2b CAR PAINT TEST 1 (the quick revert): flat paint and the roof piece always drawn (test 1's look).
inline bool carLookTest1(uint32_t value) { return value == 2; }
inline bool seatedToolsBlocked(uint32_t value) { return value != 1; }
inline bool dashPointerOn(uint32_t value) { return value != 1; }
// 0.0.74 test 2b: CRIME GPS (0 ON, 1 OFF).
inline bool crimeGpsOn(uint32_t value) { return value != 1; }
// 0.0.74 test 3 WHEEL FIRST (van072.hpp Tune::wheelFirst): 0 ON, the default; 1 OFF.
inline bool wheelFirstOn(uint32_t value) { return value != 1; }
// 0.0.74 test 10 ENTRY FACING (van072.hpp Tune::boardFaceRoom): 0 PLAY SPACE, the default; 1 HEAD.
inline bool entryFacesRoom(uint32_t value) { return value != 1; }
// 0.0.74 test 10 HIDEOUT HOLD (van_screen.hpp In::hideoutHold): 0 HOLD, the default; 1 TAP.
inline bool hideoutHoldOn(uint32_t value) { return value != 1; }
// 0.0.74 test 10 THROW BY SWING (throw_swing.hpp): 0 ON, the default; 1 OFF (the old aim).
inline bool throwSwingOn(uint32_t value) { return value != 1; }
// 0.0.74 test 10 GLIDE TILT (cape_glide.hpp): 0 ON, the default; 1 OFF (the arm poses' pitch).
inline bool glideTiltOn(uint32_t value) { return value != 1; }
// 0.0.74 test 10 QUICK HOOK, GRAPPLE YANK, SURE GRAPPLE: 0 ON, the default; 1 OFF.
inline bool quickHookOn(uint32_t value) { return value != 1; }
inline bool grappleYankOn(uint32_t value) { return value != 1; }
inline bool sureGrappleOn(uint32_t value) { return value != 1; }
inline bool beltZonesOn(uint32_t value) { return value != 1; } // 0.0.74 test 10 BELT ZONES
// Version 0.0.62 PUNCH HITS / ENEMY TRACKING values
inline constexpr uint32_t punchHitsBoth = 0, punchHitsGame = 1, punchHitsInstant = 2;
inline bool instantPunch(uint32_t value) { return value == punchHitsBoth || value == punchHitsInstant; }
inline bool foeTrackLive(uint32_t value) { return value != 1; }
// ENEMY TRACKING LIVE's check that the 30 Hz world list was measured from this hero: he is within
// 3 m + (40 m/s, or 1.5x the player's own speed when faster) x the list's age of where he was then.
// Version 0.0.62 fix: 0.0.6.2's 40 m/s alone failed for FLY MODE and SUPERSONIC (60+ m/s), and every
// frame there fell back to the old offsets that trail behind by speed x age.
inline bool foeWorldUsable(float heroGap, float age, float playerSpeed) {
    if (!std::isfinite(heroGap) || !std::isfinite(age))
        return false;
    const float speed = std::isfinite(playerSpeed) ? std::max(40.f, 1.5f * std::abs(playerSpeed)) : 40.f;
    return heroGap <= 3.f + speed * std::clamp(age, 0.f, .5f);
}
inline bool beaconsInVision(uint32_t value) { return value != 1; } // Version 0.0.62
// The instant blow (SpidyVR's 7-28 HP melee blow, scaled down beside the game's own punch, which
// still lands with GAME + INSTANT): amount 4-12 HP and knock amount 40-120 by the fist's strength
// (0-1), the knockback kind 5 the CONFIRMED repulsor blow uses (the amounts are estimates).
// INSTANT ONLY has no game punch beside it: 1.75x the amount.
struct InstantBlow {
    float amount, knockAmount;
};
inline InstantBlow instantBlowOf(float strength, uint32_t value) {
    const float s = std::isfinite(strength) ? std::clamp(strength, 0.f, 1.f) : 0.f;
    const float k = value == punchHitsInstant ? 1.75f : 1.f;
    return {(4.f + 8.f * s) * k, 40.f + 80.f * s};
}
// ---- Version 0.0.6 HERO TOGGLES --------------------------------------------------------------------
// The hero toggles (IRON MAN's is shooterType, 1 ON); only one is on at a time. Version 0.0.7: eight
// (CAPTAIN AMERICA the last).
inline constexpr Option heroToggles[10] = {shooterType, daredevil, hulk, batman, wolverine, superman, punisher, captainAmerica,
                                           thor, greenLantern}; // Version 0.0.72: THOR, GREEN LANTERN
inline bool isHeroToggle(uint32_t o) {
    for (const auto t : heroToggles)
        if (o == t)
            return true;
    return false;
}
// Version 0.0.7 values
inline bool holsterCrossDraw(uint32_t value) { return value == 1; }
inline bool batonHolstersOn(uint32_t) { return true; } // Version 0.0.72: always (no magic spawning)
inline bool batonClub(uint32_t value) { return value == 1; }
inline bool batonModel(uint32_t value) { return value == 2; } // else CLUB when the files are missing
inline bool gunModelsOn(uint32_t value) { return value == 1; }
// DAREDEVIL PUNCH's multiplier (3X, 1X, 2X, 4X, 5X).
inline float daredevilPunchK(uint32_t value) {
    static constexpr float k[5] = {3.f, 1.f, 2.f, 4.f, 5.f};
    return k[value < 5 ? value : 0];
}
// ---- Version 0.0.5 PUNCH SENSITIVITY ------------------------------------------------------------
// The 0.0.4 headset test: "punches still don't register enough". Each value scales the punch's
// speed threshold (PUNCH SPEED), its forward share (how straight at him) and its reach in front of
// the head, and says how a fist re-arms: backSpeed, the speed back toward the head that ends a blow
// (a partial pull-back is enough to punch again), and minGap, the least time between one hand's
// blows (a tracking wobble is no second punch). One X per blow, sent at once, as 0.0.4.
struct PunchFeel {
    float speedK, forwardK, reachK, backSpeed, minGap;
};
inline PunchFeel punchFeelOf(uint32_t v) {
    switch (v) {
    case 1: return {.7f, .6f, .7f, .35f, .06f};      // MAX
    case 2: return {1.f, 1.f, 1.f, .8f, 0.f};       // 0.0.4
    case 3: return {1.15f, 1.15f, 1.f, .8f, .1f};   // STRICT
    default: return {.78f, .7f, .75f, .4f, .07f};   // FORGIVING
    }
}
// ---- Version 0.0.5 SFX VOLUME -------------------------------------------------------------------
inline float sfxGainOf(uint32_t v) {
    static constexpr float gain[5] = {1.f, .75f, .5f, .25f, 0.f};
    return v < 5 ? gain[v] : 1.f;
}
// ---- Version 0.0.5 SUPERSONIC SPEED: times the FAST fly speed ------------------------------------
inline float supersonicK(uint32_t v) {
    // FAST is 40 m/s: 80, 60, 100 and 120 m/s (fly_boost.hpp; 2 m a game step at most).
    static constexpr float k[4] = {2.f, 1.5f, 2.5f, 3.f};
    return v < 4 ? k[v] : 2.f;
}
// Version 0.0.2 BALL LOOK: OURS (0) the mod's ball, no game ball; WRIST (1) and GLOB (2) the game's
// ball from the wrist (wrist_muzzle), GLOB plus our glob; GAME ONLY (3) the stock ball.
enum BallLook : uint32_t { lookOurs = 0, lookWrist = 1, lookGlob = 2, lookGameOnly = 3 };
inline bool gameBall(uint32_t look) { return look != lookOurs; }
inline bool wristMuzzle(uint32_t look) { return look == lookWrist || look == lookGlob; }
// Update 38.3 FAR WINDOWS: the percent of the game's reach each value asks for (2X, 3X, 1.5X, GAME).
inline uint32_t farWindowsPercentOf(uint32_t value) {
    static constexpr uint32_t percent[4] = {200, 300, 150, 100};
    return value < 4 ? percent[value] : 200u;
}
inline const char* valueName(uint32_t option, uint32_t value) {
    if (option >= options)
        return "?";
    const auto& o = optionInfo[option];
    return value < o.count ? o.values[value] : o.values[0];
}
inline uint32_t clampValue(uint32_t option, uint32_t value) {
    return option < options && value < optionInfo[option].count ? value : 0u;
}
// One export carries a change: index << 16 | value (SpidyCombat38).
inline constexpr uint32_t pack(uint32_t option, uint32_t value) { return option << 16 | (value & 0xffffu); }
inline constexpr uint32_t optionOf(uint32_t packed) { return packed >> 16; }
inline constexpr uint32_t valueOf(uint32_t packed) { return packed & 0xffffu; }

// ---- CRAWL CAP -------------------------------------------------------------------------
// Update 37.2 capped the head at a crouch whenever the game's floor-crawl state ran 0.4 s;
// the game sets that state in fights too (run372: the view dropped low on punches).
// Update 38 caps it ONLY under a ceiling low enough to force a crouch: a roof hit less
// than lowCeiling over the feet, on at least minSamples ray readings spanning holdSeconds,
// and never within combatQuiet of a punch or other combat input. A room's or warehouse's
// ceiling (2.4 m and up) never caps. WITH FLAG adds the 37.2 flag for a duct whose roof no
// ray hits (still never in a fight); OFF never caps.
enum CrawlMode : uint32_t { crawlAuto = 0, crawlWithFlag = 1, crawlOff = 2 };
enum CrawlWhy : uint32_t { capNone = 0, capLowCeiling = 1, capFlag = 2, heldByCombat = 3, capOff = 4 };
inline constexpr const char* crawlWhyNames[] = {"none", "low ceiling", "crawl flag (WITH FLAG)", "held off: combat",
                                                "off"};
class CrawlCap {
  public:
    static constexpr float lowCeiling = 1.6f, holdSeconds = .3f, combatQuiet = 1.5f, gapSeconds = .2f,
                           releaseSeconds = .5f, flagSeconds = .4f, neverAbove = 2.4f;
    static constexpr uint32_t minSamples = 3;
    struct Input {
        float seconds{};          // since the previous frame
        float roofMetres{};       // the roof ray's surface over the feet this frame; 0 none
        bool roofSample{};        // a fresh ray reading came this frame (hit or not)
        bool floorCrawl{};        // the game's floor crawl (WITH FLAG only)
        bool combatInput{};       // a punch, zip, web ball, gadget or the fight's buttons this frame
    };
    // True: cap the head at a crouch this frame.
    bool update(const Input& in, uint32_t mode) {
        const float dt = std::isfinite(in.seconds) ? std::clamp(in.seconds, 0.f, .1f) : 0.f;
        now_ += dt;
        if (in.combatInput)
            combatAt_ = now_;
        const bool low = std::isfinite(in.roofMetres) && in.roofMetres > 0 && in.roofMetres < lowCeiling;
        if (in.roofSample) {
            if (low) {
                if (samples_ == 0 || now_ - lastLow_ > gapSeconds) {
                    samples_ = 0;
                    lowSince_ = now_;
                }
                ++samples_;
                lastLow_ = now_;
            } else if (std::isfinite(in.roofMetres) && (in.roofMetres <= 0 || in.roofMetres >= lowCeiling)) {
                // A clear or a high reading: the low run is over.
                samples_ = 0;
                lastClear_ = now_;
            }
        }
        if (now_ - lastLow_ > gapSeconds)
            samples_ = 0;
        flagSeconds_ = in.floorCrawl ? flagSeconds_ + dt : 0.f;
        const bool lowHeld = samples_ >= minSamples && now_ - lowSince_ >= holdSeconds;
        const bool fighting = now_ - combatAt_ < combatQuiet;
        bool want = false;
        why_ = capNone;
        if (mode == crawlOff) {
            why_ = capOff;
        } else if (lowHeld || (capping_ && lastLow_ > lastClear_ && now_ - lastLow_ < releaseSeconds)) {
            want = true;
            why_ = capLowCeiling;
        } else if (mode == crawlWithFlag && flagSeconds_ >= flagSeconds) {
            want = true;
            why_ = capFlag;
        }
        if (want && fighting) {
            want = false;
            why_ = heldByCombat;
            ++heldOff_;
        }
        if (want && !capping_)
            ++caps_;
        capping_ = want;
        return want;
    }
    uint32_t why() const { return why_; }
    uint64_t caps() const { return caps_; }
    uint64_t heldOff() const { return heldOff_; }
    bool capping() const { return capping_; }
    void reset() { *this = CrawlCap{}; }

  private:
    float now_{}, combatAt_ = -1e9f, lowSince_{}, lastLow_ = -1e9f, lastClear_ = -1e9f, flagSeconds_{};
    uint32_t samples_{}, why_{};
    uint64_t caps_{}, heldOff_{};
    bool capping_{};
};

// ---- ZIP WINDOW, CONTACT, AIM ------------------------------------------------------------
inline constexpr float zipNear = 3.f, zipFar = 20.f;
inline bool inZipWindow(float metres) { return std::isfinite(metres) && metres >= zipNear && metres <= zipFar; }
// An enemy as a standing capsule from its root (its feet), tracking space, y up.
inline constexpr float contactRadius = .45f, contactLow = .2f, contactHigh = 2.1f;
inline bool touches(Vec3 hand, Vec3 enemyFeet, float radius = contactRadius) {
    if (!finite(hand) || !finite(enemyFeet))
        return false;
    const float up = hand.y - enemyFeet.y;
    if (up < contactLow || up > contactHigh)
        return false;
    const float dx = hand.x - enemyFeet.x, dz = hand.z - enemyFeet.z;
    return dx * dx + dz * dz <= radius * radius;
}
// Degrees between a ray and the line from its origin to a point (180: unknown).
inline float degreesOff(Vec3 origin, Vec3 ray, Vec3 point) {
    const Vec3 to = point - origin;
    if (!finite(to) || !finite(ray) || length(to) < 1e-3f || length(ray) < .5f)
        return 180.f;
    const float c = std::clamp(dot(normalized(to), normalized(ray)), -1.f, 1.f);
    return std::acos(c) * 57.2957795f;
}
// BALL AIM (38.2): 0 SMART (smartTarget), 1 SPIDYVR (spidyAim, combat_spidy.hpp), 2 CONE 20, 3 OFF.
// (38.1's WIDE 35 is gone: SMART covers wide near enemies.)
inline float assistCone(uint32_t value) { return value == 2 ? 20.f : 0.f; }
enum AssistMode : uint32_t { assistSmart = 0, assistSpidyVr = 1, assistCone20 = 2, assistOff = 3 };
enum ZipMode : uint32_t { zipSpidy = 0, zipBoth = 1, zipDistance = 2, zipAimed = 3 };
enum LungeMode : uint32_t { lungeGameButton = 0, lungeSpidyMove = 1 };
// The enemy a web ball bends to: the least off the wrist's ray inside `cone`, nearest first
// on a tie, within zipFar; -1 none. `chest` lifts the aim point off the feet.
template <size_t N>
inline int assistTarget(Vec3 origin, Vec3 ray, const std::array<Vec3, N>& enemies, uint32_t count, float cone,
                        float* degrees = nullptr) {
    int best = -1;
    float bestOff = cone, bestDistance = 1e9f;
    for (uint32_t i = 0; i < count && i < N; ++i) {
        const Vec3 chest = enemies[i] + Vec3{0, 1.1f, 0};
        const float d = length(chest - origin);
        if (!(d > .3f) || d > zipFar)
            continue;
        const float off = degreesOff(origin, ray, chest);
        if (off <= cone && (off < bestOff - 1.f || (off <= bestOff + 1.f && d < bestDistance))) {
            best = static_cast<int>(i);
            bestOff = off;
            bestDistance = d;
        }
    }
    if (degrees)
        *degrees = best >= 0 ? bestOff : 180.f;
    return best;
}

// Update 38.2 SMART: centre mass (centreMass over the feet), the least angle off the hand's
// ray plus `smartPerMetre` degrees for each metre away (a near enemy a little off beats a far
// one dead ahead), inside smartCone (smartNearCone within 3 m), within zipFar; the enemy
// picked before (`sticky`) keeps it unless another scores smartStickyDegrees better.
inline constexpr float smartCone = 25.f, smartNearCone = 40.f, smartPerMetre = .6f, smartStickyDegrees = 6.f,
                       centreMass = 1.05f;
template <size_t N>
inline int smartTarget(Vec3 origin, Vec3 ray, const std::array<Vec3, N>& enemies, uint32_t count, int sticky,
                       float* degrees = nullptr) {
    int best = -1;
    float bestScore = 1e9f, bestOff = 180.f;
    for (uint32_t i = 0; i < count && i < N; ++i) {
        const Vec3 chest = enemies[i] + Vec3{0, centreMass, 0};
        const float d = length(chest - origin);
        if (!(d > .3f) || d > zipFar)
            continue;
        const float off = degreesOff(origin, ray, chest);
        const float cone = d <= 3.f ? smartNearCone : smartCone;
        if (off > cone)
            continue;
        float score = off + smartPerMetre * d;
        if (static_cast<int>(i) == sticky)
            score -= smartStickyDegrees;
        if (score < bestScore) {
            best = static_cast<int>(i);
            bestScore = score;
            bestOff = off;
        }
    }
    if (degrees)
        *degrees = best >= 0 ? bestOff : 180.f;
    return best;
}

// ---- WEB BALL GLOB -----------------------------------------------------------------------
// Our own web ball: a glowing white glob that flies from the wrist along the aim (or
// toward the assisted enemy) at `speed` for at most `life`, the SM1 look; drawn over the
// game's own (gold squares in SM2's VR copy). Positions are tracking space.
struct Glob {
    static constexpr float speed = 35.f, life = .6f;
    bool alive{};
    float age{}, reach = 20.f;
    Vec3 from{}, along{};
    void launch(Vec3 wrist, Vec3 direction, float metres) {
        if (!finite(wrist) || !finite(direction) || length(direction) < .5f)
            return;
        alive = true;
        age = 0;
        from = wrist;
        along = normalized(direction);
        reach = std::isfinite(metres) ? std::clamp(metres, 1.f, 30.f) : 20.f;
    }
    void update(float seconds) {
        if (!alive)
            return;
        age += std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0.f;
        if (age >= life || age * speed >= reach)
            alive = false;
    }
    Vec3 at() const { return from + along * std::min(age * speed, reach); }
    // The trailing strand's end, back toward the wrist.
    Vec3 tail() const { return from + along * std::max(0.f, std::min(age * speed, reach) - 1.2f); }
};

// ---- HOLO OPEN ---------------------------------------------------------------------------
// Update 38.1 (wrist_hud::Toggle::setMode): 38's SAFE (0) is now TAP, LONG PRESS (1) is
// CLICK (2), F8 ONLY (2) is 3 - tools/live_settings.py maps 38's words.
enum HoloOpen : uint32_t { holoTap = 0, holoPalmLook = 1, holoClick = 2, holoF8Only = 3 };
// Update 38.2: the option's FOREARM (0) is wrist_hud::Toggle mode 4; 38.1's TAP stays in the
// Toggle (its tests) but is no longer on the option.
inline constexpr uint32_t holoForearm = 4;
inline uint32_t toggleMode(uint32_t option) { return option >= 1 && option <= 3 ? option : holoForearm; }

// ---- AIM AXIS (38.1) ---------------------------------------------------------------------
// 38 aimed along the grip's -Z, 40 degrees off where the hand points (DIAGNOSIS §0); the
// swing webs aim along webShooter(grip) -Z (the grip's -Y). The game_xr picks the pose.
enum AimAxis : uint32_t { axisHand = 0, axisAimPose = 1, axisOld = 2 };
// The direction a hand aims (Frame::Hand::aim): HAND the web shooter's -Z (webShooter, the
// grip's -Y, as the swing webs), AIM POSE the runtime's aim pose -Z, OLD 38's grip -Z.
// A pose with no usable rotation falls back to HAND.
inline Vec3 aimOf(const Pose& grip, const Pose& aimPose, uint32_t axis) {
    const auto usable = [](const Quat& q) {
        const float n = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
        return std::isfinite(n) && n > .5f && n < 1.5f;
    };
    if (axis == axisOld && usable(grip.orientation))
        return grip.orientation.rotate({0, 0, -1});
    if (axis == axisAimPose && usable(aimPose.orientation))
        return aimPose.orientation.rotate({0, 0, -1});
    // webShooter(grip, hand) (either hand: only its offset differs): {{+-.035, .05, .03}, {-.7071, 0, 0, .7071}} (tracking.hpp)
    const Quat shooter = grip.orientation * Quat{-.70710678f, 0, 0, .70710678f};
    return usable(grip.orientation) ? shooter.rotate({0, 0, -1}) : Vec3{};
}
// ---- LUNGE / PUNCH (38.1) ----------------------------------------------------------------
inline float lungeFromMetres(uint32_t v) { return v == 1 ? 1.3f : v == 2 ? 4.f : 2.5f; }
inline float lungeSpeedOf(uint32_t v) { return v == 1 ? 2.6f : v == 2 ? 1.9f : 2.2f; }
inline float punchForwardOf(uint32_t v) { return v == 1 ? .35f : v == 2 ? 0.f : .2f; }
inline constexpr float steadyAim = .7f, steadyLockHold = .5f;
// ---- BLOOD (38.1 BIG) ---------------------------------------------------------------------
inline constexpr float bigBloodLife = 1.1f, bigDropMin = .02f, bigDropMax = .05f, bigFloorLife = 2.5f;
inline constexpr uint32_t bigDrops = 20;
// Update 38.2 BRUTAL: bigger, opaque, longer and more drops, a longer floor pool and a coat
// on the punching glove; all from the same fixed pools (no allocation, no new draw call).
inline constexpr float brutalBloodLife = 2.2f, brutalDropMin = .03f, brutalDropMax = .08f, brutalFloorLife = 8.f,
                       brutalCoatLife = 12.f, brutalCoatMax = .85f;
inline constexpr uint32_t brutalDrops = 32;
enum BloodLook : uint32_t { bloodBrutal = 0, bloodBig = 1, blood37 = 2 };
// The hand coat: each landed hit adds `per` (to brutalCoatMax), fading to 0 over brutalCoatLife.
struct BloodCoat {
    static constexpr float per = .3f;
    float amount{};
    uint64_t coats{};
    void hit() {
        amount = std::min(brutalCoatMax, amount + per);
        ++coats;
    }
    void update(float dt) {
        if (std::isfinite(dt))
            amount = std::max(0.f, amount - std::clamp(dt, 0.f, .1f) * brutalCoatMax / brutalCoatLife);
    }
};

// ---- MELEE ANIM (38.2) -------------------------------------------------------------------
// The game's X combo chains its next variant - kicks among them - when X comes again within
// its combo window; a lone X after the window is the combo's first blow. PUNCH ONLY sends
// each blow as one X tap of meleeTapSeconds, at least meleeSpacing after the last tap's end,
// with the left stick neutral during the tap. CANDIDATE: the window is from run381 (X chained
// 0.2-0.6 s apart and the video's kicks), not from a confirmed game address; MELEE ANIM GAME
// is plan B.
// Version 0.0.3 (rep0023, "a lot of punches just don't land"): 0.0.2 dropped a blow that had
// waited over 0.25 s for its slot and silently ignored one asked during a tap, so any punch
// 0-0.52 s after the last tap was lost: rep0023 78 taps, 48 dropped (38% of the blows), and the
// player fell back to mashing the controller's X. Now a blow asked while the slot is busy (a tap
// down or the spacing) waits for the next slot - meleeMaxWait covers a whole tap plus the
// spacing, so no blow is dropped - and a blow asked while one already waits rides that tap
// (`merged`: the game lands one blow per X; it is counted, never lost silently).
inline constexpr float meleeTapSeconds = .12f, meleeSpacing = .65f;
inline constexpr float meleeMaxWait = meleeTapSeconds + meleeSpacing + .1f; // 0.0.2: .25 s
class MeleeSpacer {
  public:
    // `want`: a blow asked for this frame. Returns true while X is held this frame.
    bool update(float now, bool want) {
        if (!std::isfinite(now))
            return false;
        if (want) {
            if (waiting_) {
                ++merged_; // one blow already waits for the next slot: this one rides it
            } else {
                waiting_ = true;
                askedAt_ = now;
            }
        }
        if (waiting_) {
            if (!tapping(now) && now >= until_ + meleeSpacing) {
                waiting_ = false;
                delayed_ += now - askedAt_ > .001f;
                worst_ = std::max(worst_, now - askedAt_); // Version 0.0.4: the delay, measured
                delaySum_ += static_cast<double>(now - askedAt_);
                from_ = now;
                until_ = now + meleeTapSeconds;
                ++taps_;
            } else if (now - askedAt_ > meleeMaxWait || now < askedAt_) {
                waiting_ = false; // only a clock jump gets here (the slot is meleeMaxWait away at most)
                ++dropped_;
            }
        }
        return tapping(now);
    }
    bool tapping(float now) const { return now >= from_ && now < until_; }
    bool waiting() const { return waiting_; }
    uint64_t taps() const { return taps_; }
    uint64_t dropped() const { return dropped_; }
    uint64_t delayed() const { return delayed_; }
    uint64_t merged() const { return merged_; }
    float worstDelay() const { return worst_; }
    float meanDelay() const { return taps_ ? static_cast<float>(delaySum_ / static_cast<double>(taps_)) : 0.f; }

  private:
    float from_ = -1e9f, until_ = -1e9f, askedAt_{}, worst_{};
    double delaySum_{};
    bool waiting_{};
    uint64_t taps_{}, dropped_{}, delayed_{}, merged_{};
};

// ---- Version 0.0.4 MELEE ANIM EVERY HIT ------------------------------------------------------
// rep0030 ("hardly any register ... i punch fast it needs to register every single hit"): the
// spacer above sends one X per 0.77 s at most, so a fast puncher's blows rode one tap (73 of
// 216 blows merged, 95 waited up to 0.77 s). EVERY HIT sends each blow its own short X tap at
// once: down tapDown, then up at least tapUp so the game sees a fresh press (two of its frames
// at 35 fps each). A blow asked while a tap is down or in its up gap waits for the next slot
// (at most tapDown + tapUp = 0.13 s); none rides another tap and none is dropped - up to
// `most` wait in order (a faster burst than 7.7 a second is the only way to fill it; the
// overflow is counted, `merged`). CONFIRMED: an X press is one blow of the game's melee
// (run38-rep0030 pad logs). The game still picks its combo's animation, kicks among them
// (no confirmed address to choose it); the stick stays neutral during each tap.
inline constexpr float tapDown = .07f, tapUp = .06f;
class PunchTaps {
  public:
    static constexpr uint32_t most = 6;
    bool update(float now, bool want) {
        if (!std::isfinite(now))
            return false;
        if (now < last_) { // a clock jump: start afresh, nothing waits on the old clock
            pending_ = 0;
            from_ = until_ = -1e9f;
        }
        last_ = now;
        if (want) {
            if (pending_ < most) {
                asked_[(head_ + pending_) % most] = now;
                ++pending_;
            } else {
                ++merged_;
            }
        }
        if (pending_ && !tapping(now) && now >= until_ + tapUp) {
            const float delay = now - asked_[head_];
            head_ = (head_ + 1) % most;
            --pending_;
            delayed_ += delay > .001f;
            worst_ = std::max(worst_, delay);
            delaySum_ += static_cast<double>(delay);
            from_ = now;
            until_ = now + tapDown;
            ++taps_;
        }
        return tapping(now);
    }
    // Play stopped (a menu, a cutscene): a blow still waiting is not thrown into the next fight.
    void clear() {
        cleared_ += pending_;
        pending_ = 0;
        from_ = until_ = -1e9f;
    }
    uint64_t cleared() const { return cleared_; }
    bool tapping(float now) const { return now >= from_ && now < until_; }
    uint32_t waiting() const { return pending_; }
    uint64_t taps() const { return taps_; }
    uint64_t merged() const { return merged_; }
    uint64_t dropped() const { return 0; }
    uint64_t delayed() const { return delayed_; }
    float worstDelay() const { return worst_; }
    float meanDelay() const { return taps_ ? static_cast<float>(delaySum_ / static_cast<double>(taps_)) : 0.f; }

  private:
    float from_ = -1e9f, until_ = -1e9f, last_ = -1e9f, worst_{};
    float asked_[most]{};
    uint32_t head_{}, pending_{};
    double delaySum_{};
    uint64_t taps_{}, merged_{}, delayed_{}, cleared_{};
};

// ---- GADGET KEYS (38.2) ------------------------------------------------------------------
// The slots (combat_input::Slot: 1 RB+A, 2 RB+B, 3 RB+X, 4 RB+Y) per gadget. 38.2: the Grabber
// RB+B (CONFIRMED by the run381 video: the Sonic Burst's RB+B brought out the Web Grabber),
// Sonic RB+X, Upshot RB+Y, Ricochet RB+A (CANDIDATE: the game's layout with the Grabber and
// Sonic as the video showed). GAME DEFAULT: Spider-Man 2's published layout (R1+Square
// Grabber, Circle Sonic, Triangle Upshot, Cross Ricochet: Xbox X/B/Y/A). CUSTOM: live.txt's.
// SLOT TEST on the hologram confirms each in one session.
struct GadgetSlots {
    uint32_t grabber, sonic, upshot, ricochet;
};
inline GadgetSlots gadgetSlots(uint32_t keys, GadgetSlots custom) {
    if (keys == 0)
        return {2, 3, 4, 1};
    if (keys == 1)
        return {3, 2, 4, 1};
    return custom;
}

// ---- GRAB WEB (38.2) ---------------------------------------------------------------------
// SM2's swing webs only meet the world's collision (game_swing.cpp: sm2::notPorted keeps the
// grab of characters off), so a grab web went through an enemy. ENEMY: the grip's web ray
// tested against each enemy's standing capsule (feet+grabLow to feet+grabHigh, grabRadius)
// within grabReach; the nearest hit is the web's.
inline constexpr float grabRadius = .45f, grabLow = .1f, grabHigh = 1.85f, grabReach = 25.f;
// The distance along `ray` from `origin` to the capsule of an enemy standing at `feet` (y up),
// or -1 for a miss.
inline float rayCapsule(Vec3 origin, Vec3 ray, Vec3 feet, float radius = grabRadius) {
    if (!finite(origin) || !finite(ray) || !finite(feet) || length(ray) < .5f)
        return -1;
    const Vec3 d = normalized(ray);
    const Vec3 a = feet + Vec3{0, grabLow, 0};
    const float h = grabHigh - grabLow;
    // Closest points between the ray origin+d*t and the vertical segment a+(0,s,0), s in [0,h].
    const Vec3 w = origin - a;
    const float dy = d.y, den = 1.f - dy * dy;
    float t;
    if (den > 1e-5f) {
        // minimise |w + d t - (0,s,0)|: s = w.y + dy t; t from the horizontal part
        t = -(w.x * d.x + w.z * d.z) / den;
    } else {
        t = 0;
    }
    float best = -1, bestDist = 1e9f;
    const auto tryT = [&](float tt) {
        if (tt < 0 || tt > grabReach)
            return;
        const Vec3 p = origin + d * tt;
        const float s = std::clamp(p.y - a.y, 0.f, h);
        const float dist = length(p - (a + Vec3{0, s, 0}));
        if (dist < bestDist) {
            bestDist = dist;
            best = tt;
        }
    };
    tryT(t);
    // the segment's ends (the ray may pass over a head or under the feet)
    for (const float s : {0.f, h}) {
        const Vec3 e = a + Vec3{0, s, 0};
        tryT(dot(e - origin, d));
    }
    tryT(0.f);
    return bestDist <= radius ? best : -1.f;
}
template <size_t N>
inline int grabTarget(Vec3 origin, Vec3 ray, const std::array<Vec3, N>& enemies, uint32_t count, float* metres = nullptr) {
    int best = -1;
    float bestT = 1e9f;
    for (uint32_t i = 0; i < count && i < N; ++i) {
        const float t = rayCapsule(origin, ray, enemies[i]);
        if (t >= 0 && t < bestT) {
            best = static_cast<int>(i);
            bestT = t;
        }
    }
    if (metres)
        *metres = best >= 0 ? bestT : -1.f;
    return best;
}
// Update 38.3 GRAB WEB ASSIST: the ray's capsule first (EXACT, 38.2's); when it misses, the enemy
// least off the ray within grabAssistDegrees and grabAssistReach (centre mass, nearest on a tie):
// run382 attached 17 grabs in a whole fight, the ray had to pass within 0.45 m of a body.
// *assisted: the cone took him. On the ground only (game_xr): a swing web in the air stays a swing.
inline constexpr float grabAssistDegrees = 12.f, grabAssistReach = 15.f;
template <size_t N>
inline int grabAssistTarget(Vec3 origin, Vec3 ray, const std::array<Vec3, N>& enemies, uint32_t count, bool assist,
                            float* metres = nullptr, bool* assisted = nullptr) {
    if (assisted)
        *assisted = false;
    const int exact = grabTarget(origin, ray, enemies, count, metres);
    if (exact >= 0 || !assist)
        return exact;
    int best = -1;
    float bestOff = grabAssistDegrees, bestDistance = 1e9f;
    for (uint32_t i = 0; i < count && i < N; ++i) {
        const Vec3 chest = enemies[i] + Vec3{0, centreMass, 0};
        const float d = length(chest - origin);
        if (!(d > .3f) || d > grabAssistReach)
            continue;
        const float off = degreesOff(origin, ray, chest);
        if (off <= bestOff + .5f && (off < bestOff - .5f || d < bestDistance)) {
            best = static_cast<int>(i);
            bestOff = std::min(off, bestOff);
            bestDistance = d;
        }
    }
    if (best >= 0) {
        if (metres)
            *metres = bestDistance;
        if (assisted)
            *assisted = true;
    }
    return best;
}
// The grab held on an enemy: released with the grip, the enemy gone or after holdSeconds; a
// sharp pull of the hand toward the body (yankSpeed, head-relative) is the yank, once per grab.
struct GrabWeb {
    // Update 38.3: yankSpeed 1.0 (was 1.2: 9 yanks in 17 grabs, run382).
    static constexpr float yankSpeed = 1.0f, holdSeconds = 4.f;
    bool on{}, yanked{};
    int enemy = -1;
    float age{};
    Vec3 at{};
    uint64_t attached{}, yanks{}, released{};
    void attach(int e, Vec3 point) {
        on = true;
        yanked = false;
        enemy = e;
        at = point;
        age = 0;
        ++attached;
    }
    void release() {
        if (on)
            ++released;
        on = false;
        enemy = -1;
    }
    // True the frame the pull yanks. `toBody`: from the hand toward the chest.
    bool update(float dt, bool gripHeld, bool enemyPresent, Vec3 handVelocityRelHead, Vec3 toBody) {
        if (!on)
            return false;
        age += std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        if (!gripHeld || !enemyPresent || age > holdSeconds) {
            release();
            return false;
        }
        if (!yanked && length(toBody) > .05f && finite(handVelocityRelHead) &&
            dot(handVelocityRelHead, normalized(toBody)) >= yankSpeed) {
            yanked = true;
            ++yanks;
            return true;
        }
        return false;
    }
};

// ---- GADGET HINT (Update 38.3) ------------------------------------------------------------
// The user: "the gadgets aren't intuitive". The gadget each gesture fired, named over the
// firing wrist for `life` seconds (pixel font, the eyes only), and after a Sonic Burst the pose
// missed, how to make it (at most once per sonicEvery). GADGET HINT OFF: none.
enum HintLabel : uint32_t { hintNone = 0, hintUpshot, hintGrabber, hintSonic, hintRicochet, hintYank, hintGrab,
                            hintSonicHow, hintLabels };
inline const char* hintText(uint32_t label) {
    switch (label) {
    case hintUpshot: return "UPSHOT";
    case hintGrabber: return "GRABBER";
    case hintSonic: return "SONIC BURST";
    case hintRicochet: return "RICOCHET";
    case hintYank: return "YANK";
    case hintGrab: return "GRAB - PULL TO YANK";
    case hintSonicHow: return "SONIC: BOTH HANDS OUT IN FRONT";
    default: return "";
    }
}
struct GadgetHint {
    static constexpr float life = .9f, sonicEvery = 3.f;
    uint32_t label = hintNone;
    float age = life;
    void show(uint32_t l) {
        label = l;
        age = 0;
    }
    void update(float dt) { age += std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f; }
    // 1 while shown, fading over the last third; 0 none.
    float fade() const {
        if (label == hintNone || !(age < life))
            return 0.f;
        return std::clamp((life - age) / (life / 3.f), 0.f, 1.f);
    }
};

// ---- WALL TILT ---------------------------------------------------------------------------
// The view's wall-crawl roll/pitch: SAFE levels it on a death or a respawn, and with no
// wall contact never lets it pass 60 degrees (a tilt left over from a wall, run372).
inline constexpr float tiltCapDegrees = 60.f;
struct TiltGuard {
    uint64_t resets{}, capped{};
    // Returns the tilt angle (degrees) allowed this frame; `reset` set when it must level now.
    float guard(float degrees, bool onWall, bool diedOrRespawned, uint32_t mode, bool& reset) {
        reset = false;
        if (mode != 0 || !std::isfinite(degrees))
            return degrees;
        if (diedOrRespawned) {
            reset = true;
            ++resets;
            return 0.f;
        }
        if (!onWall && std::abs(degrees) > tiltCapDegrees) {
            ++capped;
            return std::copysign(tiltCapDegrees, degrees);
        }
        return degrees;
    }
};

// ---- RING ANCHOR -------------------------------------------------------------------------
// The ring is drawn into an eye image that may be a frame or more old (image reuse), at the
// wrist that image's frame had; the hand moved on since, so the ring trails it. HAND puts
// the newest tracked wrist into that image's world (the image's world-from-tracking pose
// applied to the newest wrist's tracking pose); WORLD keeps 37.2's wrist. `error`: how far
// apart the two are (metres), the ring-to-wrist drift HAND removes.
inline Pose ringWrist(const Pose& imageWrist, const Pose& imageWorldFromTracking, const Pose& latestToWorld,
                      const Pose& latestWrist, uint32_t mode, float* error = nullptr) {
    const Pose hand = compose(imageWorldFromTracking, compose(inverse(latestToWorld), latestWrist));
    const bool usable = finite(hand.position) && std::isfinite(hand.orientation.w) &&
                        std::isfinite(hand.orientation.x) && std::isfinite(hand.orientation.y) &&
                        std::isfinite(hand.orientation.z) && finite(imageWrist.position);
    if (error)
        *error = usable ? length(hand.position - imageWrist.position) : 0.f;
    return mode == 0 && usable ? hand : imageWrist;
}

// ---- RING LAYER (38.1) -------------------------------------------------------------------
// NEWEST: the ring and hologram at the newest tracked hand mapped into the image's world
// (ringWrist, the hologram too now); hidden when the image is older than the newest game
// frame by `ringStaleSeconds`, or its world jumped more than `ringJumpMetres` (a teleport,
// a respawn: DIAGNOSIS §1). IMAGE: 38's (the image's own grip). NOT a headset-rate quad
// layer: an image the headset reuses keeps what was drawn into it.
inline constexpr float ringStaleSeconds = .05f, ringJumpMetres = 2.f;
enum RingLayer : uint32_t { layerHold = 0, layerNewest = 1, layerImage = 2 };
enum RingAnchor : uint32_t { anchorWrist = 0, anchorHand = 1, anchorWorld = 2 };
// ---- RING WRIST (38.2) -------------------------------------------------------------------
// run381 (DIAGNOSIS §3): the ring rode the controller, not the drawn wrist -
//  (a) HAND put it at the NEWEST pose while the glove in the picture is at the image's
//      (17.4 cm apart at peak, ring_error_peak_cm);
//  (b) the game's arm is shorter (arm_scale): reaching out, the drawn wrist stops short of
//      the controller (body hand_error_m to .23 m) and the ring stayed on the controller,
//      over the hand.
// WRIST: the drawn wrist's web shooter (tracking.hpp webStart's spot) from the IMAGE's grip,
// pulled back toward the shoulder by the body's reach shortfall (native_body handError),
// in the frame health_ring places in (the aim pose: grip * Rx(-60), run381's fit).
inline constexpr float aimFromGripDegrees = 60.f; // run381: aim = grip * Rx(-60) (0.3 mm median)
inline Quat gripToAim() {
    const float h = -aimFromGripDegrees * .5f * .0174532925f;
    return Quat{std::sin(h), 0, 0, std::cos(h)};
}
inline Vec3 webShooterSpot(const Pose& grip, unsigned hand) {
    const float palm = hand == 0 ? .01f : -.01f;
    return compose(grip, Pose{{palm, .105f, 0}, {0, 0, 0, 1}}).position;
}
// The drawn wrist: the controller's spot moved `shortfall` metres toward the shoulder.
inline Vec3 drawnWrist(Vec3 spot, Vec3 shoulder, float shortfall) {
    if (!(shortfall > 0) || !std::isfinite(shortfall) || !finite(shoulder))
        return spot;
    const Vec3 to = shoulder - spot;
    const float l = length(to);
    if (!(l > .1f))
        return spot;
    return spot + to / l * std::min(std::min(shortfall, .3f), l - .1f);
}
// The shoulder from the head (y up): 20 cm under it, 17 cm to the side, 5 cm back.
inline Vec3 shoulderOf(const Pose& head, unsigned hand) {
    const Vec3 fwd = head.orientation.rotate({0, 0, -1});
    Vec3 flat{fwd.x, 0, fwd.z};
    const float l = length(flat);
    flat = l > 1e-3f ? flat / l : Vec3{0, 0, -1};
    const Vec3 right{-flat.z, 0, flat.x};
    return head.position + Vec3{0, -.20f, 0} + right * (hand == 0 ? -.17f : .17f) - flat * .05f;
}
// The ring's pose for health_ring::append (its centre lands on the wrist's spot): `centre`
// is the ring's centre in the aim frame health_ring offsets by.
inline Pose ringAtWrist(const Pose& grip, unsigned hand, Vec3 shoulder, float shortfall, Vec3 centre) {
    const Quat aim = grip.orientation * gripToAim();
    const Vec3 spot = drawnWrist(webShooterSpot(grip, hand), shoulder, shortfall);
    return Pose{spot - aim.rotate(centre), aim};
}
// HOLD: the last good pose kept through a bad one (never hidden); a good one taken as it is
// (`follow` 1: easing lagged the wrist by up to 1.5 cm at 2 m/s), a jump past ringJumpMetres too.
struct RingHold {
    static constexpr float follow = 1.f;
    bool has{};
    Pose pose{};
    uint64_t held{};
    Pose update(const Pose& want, bool good) {
        const bool ok = good && finite(want.position) && std::isfinite(want.orientation.w);
        if (!ok) {
            held += has;
            return pose;
        }
        if (!has || length(want.position - pose.position) > ringJumpMetres) {
            pose = want;
            has = true;
            return pose;
        }
        pose.position = pose.position + (want.position - pose.position) * follow;
        pose.orientation = want.orientation;
        return pose;
    }
};
inline bool ringStale(float imageAgeSeconds, float jumpMetres) {
    return !std::isfinite(imageAgeSeconds) || !std::isfinite(jumpMetres) || imageAgeSeconds > ringStaleSeconds ||
           jumpMetres > ringJumpMetres;
}

// ---- TELEMETRY (SpidyCombat38Data, tools/update37_records.py 'combat38') ------------------
enum Reject : uint32_t {
    sent = 0, untracked, notFree, triggerHeld, busy, tooSlow, notForward, notExtended, cooldown, gated, rejects
};
inline constexpr const char* rejectNames[rejects] = {"sent",       "hand untracked", "hand holding or carrying",
                                                     "trigger held", "hand busy",    "too slow",
                                                     "not forward", "not extended", "not re-armed",
                                                     "gated (not playing)"};
inline constexpr uint32_t dataMagic = 0x38424d43; // CMB8
inline constexpr uint32_t maxEnemies = 16;
struct Data {
    // Version 2 (the SpidyVR fold-in): 424 bytes, the lunge, yank, lock and sight counts.
    // Version 4 (38.2): 664 bytes, version 3's fields at the same offsets up to 504.
    // Version 5 (38.3): 768 bytes, version 4's fields at the same offsets up to 656.
    // Version 3 (38.1): 512 bytes, version 2's fields at the same offsets up to 416, the new
    // ones after them and `sequence` last again (the seqlock readers check its last 8 bytes).
    uint32_t magic = dataMagic, version = 5, bytes = 768, flags{};   // flags: 1 airborne combat allowed
    uint32_t values[12]{};                                            // the options 0-11 in force
    uint64_t rejects[combat38::rejects]{};                            // punch-candidate frames by reason
    uint64_t contactHits{}, gestureHits{}, zips{}, zipsAimed{}, airPunches{}, assists{}, assistsMissed{}, globs{};
    uint64_t crawlCaps{}, crawlHeldOff{}, holoDeferred{}, holoCancelled{}, tiltResets{}, tiltCapped{};
    // NPC NEAR FADE for enemies (npc_fade's own record counts people only): episodes within 2 m and
    // frames within 2 m, by the fade mode in force then (0 GAME, 1 OFF, 2 SWITCH).
    uint64_t enemyNear[3]{}, enemyNearFrames[3]{};
    uint32_t enemies{}, crawlWhy{}, lastReject{}, npcMode{};
    float nearestEnemy = 1e9f, assistDegrees = 180.f, ringError{}, scanHz{};
    float ringErrorPeak{}, roofMetres{};
    // Version 0.0.3 (offset 344, was a spare pad[2]): PUNCH ONLY blows that rode a tap already
    // waiting (MeleeSpacer::merged), and TARGETED swings held off with an enemy in sight range
    // but past targetReach (the rest of targetedHeldOff had no enemy at all, or a menu open).
    uint32_t meleeMerged{}, heldOffFar{};
    // Version 2: SpidyVR lunges opened / landed (the X on arrival) / missed, yanks, the
    // head-look lock's changes, sight rays cast / blocked, and lunges and balls that
    // went without a sight reading (unknown counts as in sight).
    uint64_t lunges{}, lungesLanded{}, lungesMissed{}, yanks{}, lockChanges{}, sightRays{}, sightBlocked{},
        sightUnknown{};
    // Version 3 (offset 416): options 12-18 in force (values2[i] is option 12 + i), swings
    // rejected (one per swing, its reason at the swing's peak), the holo zone's swallowed
    // presses / taps / PALM LOOK opens, the ring and hologram hidden as stale (frames), STEADY
    // lunges held off (no visible locked target: punched instead), and lock switches the
    // 0.5 s hold kept back.
    uint32_t values2[8]{};
    uint64_t swingRejects{}, zoneSwallowed{}, holoTaps{}, holoLookOpens{}, ringHidden{}, lungeHeldOff{};
    uint32_t lockHeld{}, aimStrengthNow{}; // Version 0.0.2: option 28 (AIM STRENGTH) in force (was spare)
    // Version 4 (38.2, offset 504): options 20-23 (values3[i] is option 20 + i, 4-7 spare; option 19 is
    // values2[7]), the holo FOREARM opens, ring frames held (HOLD), TARGETED attacks held off
    // (no enemy in reach or locked), PUNCH ONLY taps sent / delayed / dropped, gadgets EASY
    // recognised (each buzzed), grab webs attached / yanked / released, SMART assists, BRUTAL
    // hand coats, the mirror's SUIT frames, the ring's distance from the drawn wrist now /
    // peak (cm) and the SUIT's joints in its last frame.
    uint32_t values3[8]{};
    uint64_t forearmOpens{}, ringHeld{}, targetedHeldOff{}, meleeTaps{}, meleeDelayed{}, meleeDropped{};
    uint64_t gadgetsRecognised{}, grabAttached{}, grabYanks{}, grabReleased{}, smartAssists{}, bloodCoats{};
    uint64_t mirrorSuitFrames{};
    float ringWristCm{}, ringWristPeakCm{};
    uint32_t mirrorJoints{}, ballHitNow{}; // Version 0.0.2: option 29 (BALL HIT) in force (was spare)
    // Version 5 (38.3, offset 656): QUICK lock switches, strikes steered to the lock, grab webs
    // the ASSIST cone attached (the ray itself missed), GADGET HINT labels shown, Sonic aiming
    // hints shown after a miss, yank gestures sent; FAR WINDOWS: the reach hook's calls, the
    // reach writes for the game's own view, its own reach (m) in the last call and ours.
    uint64_t lookSwitches{}, strikeSteers{}, grabAssisted{}, gadgetHints{}, sonicHints{}, yankGestures{};
    uint64_t windowCalls{}, windowWrites{};
    float windowGameMetres{}, windowMetres{};
    // MIRROR SUIT COLOURS (offset 728): the suit pixels sampled from the left eye's own copies
    // and the copies they came from; the learned primary / secondary / emblem colours as
    // 0xRRGGBB (sRGB), and 1 once they are learned (before that the default colours show).
    uint64_t suitSamples{}, suitSnapshots{};
    uint32_t suitPrimary{}, suitSecondary{}, suitEmblem{}, suitLearned{};
    uint64_t sequence{};
};
static_assert(sizeof(Data) == 768);
static_assert(offsetof(Data, meleeMerged) == 344 && offsetof(Data, heldOffFar) == 348);
static_assert(offsetof(Data, values2) == 416 && offsetof(Data, swingRejects) == 448 && offsetof(Data, values3) == 504 &&
              offsetof(Data, forearmOpens) == 536 && offsetof(Data, ringWristCm) == 640 &&
              offsetof(Data, mirrorJoints) == 648 && offsetof(Data, lookSwitches) == 656 &&
              offsetof(Data, windowCalls) == 704 && offsetof(Data, windowGameMetres) == 720 &&
              offsetof(Data, suitSamples) == 728 && offsetof(Data, suitPrimary) == 744 &&
              offsetof(Data, sequence) == 760);
} // namespace spidy::combat38
