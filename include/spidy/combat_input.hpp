#pragma once
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
//
// Update 37: combat with the VR controllers - the web shooter and the four gadgets by
// gesture, and fast hand hits - sent to the game as ITS OWN pad input (the virtual XInput
// controller, game_pad.hpp), never through invented addresses.
//
// What is confirmed and what is a candidate (Spider-Man2.exe 2.810.0.0, October 24):
// - CONFIRMED: the game reads the virtual pad (game_pad.hpp: menus, walking, dodge, heal
//   and prompts already go through it).
// - CONFIRMED (exe strings, the game's own control names): KEY_BINDING_SHOOT_GADGET,
//   KEY_BINDING_GADGET_MODIFIER, KEY_BINDING_GADGET_WEBGRABBER, ..._GADGET_CONCUSSIONBURST
//   (the Sonic Burst; "Sonic Burst" is a string too), ..._GADGET_UPSHOT,
//   ..._GADGET_RICOCHETWEB, KEY_BINDING_ATTACK; and "Web Shooter Weapon". So Spider-Man 2
//   fires a gadget as a held modifier plus that gadget's own button, and the four are the
//   Web Grabber, Sonic (concussion) Burst, Upshot and Ricochet Web. "Impact Web" is only an
//   older game's leftover (UIMenuGadgetImpactWebShow, "Possibly Obsolete" notes): not used.
// - CANDIDATE: which PAD buttons those are: the web shooter RB tapped, a gadget RB held
//   plus a face button. Each gadget's face button is a live setting (grabber_slot,
//   sonic_slot, upshot_slot, ricochet_slot: RB+A/B/X/Y, or 0 = that gesture off), so a
//   wrong guess is fixed in live.txt without a new build (plan B).
// - NOT PORTED: per-enemy hits and the shot's own origin (SpidyVR Remastered's
//   game_punch / game_shooter use Remastered addresses; Spider-Man 2's are not evidenced).
//   A fast hand hit is the game's attack button (X), which aims at the nearest enemy itself.
//
// The gestures (each forgiving: LOOSENESS scales every threshold the easy way):
//   WEB SHOOTER   tap the trigger (let go within TAP) -> RB tapped
//   WEB GRABBER   hold the trigger, yank the hand back toward the body -> RB + its button
//   UPSHOT        aim low (the wrist's ray under UPSHOT LOW), hold the trigger, swing the
//                 hand up -> RB + its button
//   RICOCHET WEB  hold the trigger, then the grip with it, flick the wrist forward
//   SONIC BURST   both hands forward, close side by side, both triggers within the grace
//   HAND HIT      a fast forward hand, no trigger held -> X
//   ZIP STRIKE    (Update 37.2) the same punch while the nearest enemy is 5-10 m away (the
//                 WORLD LOG's read-only nearest distance) -> Y, the game's web strike (CANDIDATE:
//                 Y; the hologram's SLOT TEST checks it). Unknown distance: a normal hit (X).
//   WEB YANK      (Update 37.2, live.txt web_yank, default OFF) trigger held, then the grip, and a
//                 sharp pull back toward the body -> yank_slot (Update 38.5: Y HOLD, the game's
//                 published hold Triangle - CANDIDATE; plan B RB HOLD; SLOT TEST).
// Update 37.2: no gadget while any web is attached (the swing's own triggers), the Sonic
// Burst needs both hands out at chest height and has a cooldown, and the hand speeds ignore
// timing glitches and tracking jumps (they read 57,000 m/s in run371 and fired false hits).
// Priority: Sonic Burst (both triggers within the grace) > Ricochet (grip with it) >
// Upshot (aimed low) > Web Grabber > the tap. A held trigger that makes no gesture within
// GESTURE TIME does nothing when let go (a hold is unassigned). One gesture per press.
//
// The rule against clashes with swinging: only a FREE hand counts - no web attached (its
// trigger reels), no trigger the temple scan, the hologram or a game prompt claimed, not a
// hand holding the wall to climb, and only in VR play (no menu, phone, pad layer or
// calibration). While a hand's trigger is held for a gesture, its grip shoots no swing web
// (holdsGrip), so Ricochet's grip never fires a web too.
//
// Update 38, SpidyVR's combat folded in (combat_spidy.hpp, pure logic, no new address):
// - PUNCHES: SpidyVR's busy rule - only a hand whose OWN web holds something or carries the
//   player (or that holds a grab or the wall) is kept from punching; a hand whose web just
//   hangs from a wall while the player stands punches. The speed is the hand's relative to
//   the head (the arm's own motion, not the body's swing), and a hand re-arms once it slows
//   under 1.2 m/s or 0.45 s after its hit (SpidyVR's rule, instead of 37.2's 0.30 s
//   cooldown). CONTACT is SpidyVR's swept 8 cm fist into each enemy's body capsule at
//   2.2 m/s or more, into his middle or head.
// - LOCK: the enemy the head looks at within 40 degrees and 10 m, held until another is
//   looked at squarely for 0.3 s; punches go toward him, a lunge goes only at him.
// - ZIP MODE SPIDYVR: SpidyVR's lunge - a fist thrown out from the neck at 2.6 m/s for 40 ms
//   at the locked enemy 1.3-10 m away, within 37/60 degrees, in sight - goes out as the
//   game's own web strike (Y, LUNGE GAME BUTTON), or as SpidyVR's own flight in the swing
//   bridge (LUNGE SPIDYVR MOVE, experimental) with the game's X on arrival.
// - BALL AIM SPIDYVR: Shooter::aim's assist - the enemy nearest the hand's ray as a share of
//   about 7 degrees or his width, within 45 m, no wall between (sight rays).
// - WEB GRABBER and WEB YANK: SpidyVR's yank - 14 cm back along the shot within 0.25 s at
//   1.3 m/s or more, head-relative.
// - BALL FIRE PRESS: the web ball goes on the trigger's press (after the burst's short grace,
//   at most every 0.12 s), as SpidyVR's shooter; RELEASE keeps 37.2's tap.
#include "combat38.hpp"
#include "combat_spidy.hpp"
#include "math.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace spidy::combat_input {
// XINPUT_GAMEPAD bits (game_pad::Button).
inline constexpr uint16_t padA = 0x1000, padB = 0x2000, padX = 0x4000, padY = 0x8000, padLb = 0x100, padRb = 0x200;
// A gadget's binding: 0 off, 1-4 RB + A/B/X/Y.
// Update 37.2: 5 RB held (the yank's old candidate), 6 Y alone (the web strike's candidate).
// Update 38.5: 7 Y held (the yank's default: the game's published "Hold Triangle" for its web
// yank, throw and disarm - CANDIDATE until a session shows it yanking; run384 sent RB HOLD 0 times
// as a yank). Y held in the air may open the game's Web Wings (that is the game's own move).
enum Slot : uint32_t { slotOff = 0, rbA, rbB, rbX, rbY, rbHold, yAlone, yHold, slots };
inline constexpr const char* slotNames[slots] = {"OFF", "RB+A", "RB+B", "RB+X", "RB+Y", "RB HOLD", "Y", "Y HOLD"};
inline const char* slotName(uint32_t s) { return s < slots ? slotNames[s] : "?"; }
inline uint16_t slotFace(uint32_t s) {
    const uint16_t face[] = {0, padA, padB, padX, padY, 0, padY, padY};
    return s < slots ? face[s] : 0;
}
enum Gadget : uint32_t { none = 0, webShooter, webGrabber, sonicBurst, upshot, ricochetWeb, webYank, zipStrike, gadgets };
inline constexpr const char* gadgetNames[gadgets] = {"none", "Web Shooter", "Web Grabber", "Sonic Burst",
                                                     "Upshot", "Ricochet Web", "Web Yank", "Zip Strike"};

struct Config {
    bool on = true;                 // live.txt combat
    uint32_t tapMs = 300;           // tap_ms: let go within this is the web shooter's tap
    uint32_t gestureMs = 1200;      // gesture_ms: how long a held trigger waits for a gesture
    // Each gadget's button with RB (CANDIDATE defaults; 0 turns that gesture off).
    uint32_t grabberSlot = rbY, sonicSlot = rbB, upshotSlot = rbA, ricochetSlot = rbX;
    float yankSpeed = 1.6f;         // yank_speed (m/s) toward the body: Web Grabber
    float upSpeed = 1.6f;           // upshot_speed (m/s) upward: Upshot
    float lowDegrees = 15.f;        // upshot_low: the wrist aimed at least this far under level
    float flickSpeed = 1.8f;        // flick_speed (m/s) forward along the aim: Ricochet Web
    float blastGap = .25f;          // sonic_gap (m): the hands at most this far apart
    float blastReach = .30f;        // sonic_reach (m): each hand this far in front of the head
    bool blastPalms = false;        // also the palms facing forward (grip axes: candidate)
    bool punch = true;              // punch
    float punchSpeed = 2.4f;        // punch_speed (m/s); Update 37.2: 2.4 (was 3: hits too hard to land)
    float punchReach = .20f;        // a punch counts this far in front of the head (was .25)
    // Update 37.2: ZIP STRIKE and WEB YANK (see above).
    bool zipStrike = true;          // zip_strike
    // Update 38: 3-20 m (was 5-10: Zip Strike 0 in run372, the enemies were further).
    float zipNear = combat38::zipNear, zipFar = combat38::zipFar; // the nearest enemy's distance that makes a punch a zip
    // Update 38 zip_mode: 0 SPIDYVR (the lunge at the locked enemy), 1 BOTH, 2 DISTANCE,
    // 3 AIMED (the wrist on a character). SPIDYVR with no enemy list (WORLD LOG off): AIMED.
    uint32_t zipMode = combat38::zipSpidy;
    uint32_t punchMode = 0;         // Update 38 punch_mode: 0 BOTH, 1 GESTURE (fast punches), 2 CONTACT (touch)
    bool assistSpidy = true;        // Update 38 aim_assist SPIDYVR (Shooter::aim's rule)
    float assistDegrees = 0.f;      // Update 38 aim_assist CONE 20 / WIDE 35: the web ball's cone (0 none)
    uint32_t lungeMode = combat38::lungeGameButton; // Update 38 lunge: 0 GAME BUTTON (Y), 1 SPIDYVR MOVE
    bool ballOnPress = true;        // Update 38 ball_fire: PRESS (SpidyVR) or RELEASE (37.2's tap)
    // Version 0.0.2 BALL LOOK OURS: a web-shooter press is the mod's own ball only (our_ball.hpp,
    // Out::ballOurs) - no RB, no stick: the game fires no web ball for it.
    bool ballOurs = false;
    // Version 0.0.4 IRON MAN ABILITIES (REPULSOR) (repulsor.hpp): the trigger makes the shot only - no
    // Grabber, Upshot, Ricochet, yank or Sonic Burst from it (all web or the game's).
    bool noTriggerGadgets = false;
    // Update 38.1 (run38/DIAGNOSIS.md §2-4); these defaults are 38's (the game_xr sets 38.1's
    // from the COMBAT settings): the nearest locked enemy (from the chest) a swing lunges at,
    // the lunge's swing speed, a fast hand's forward share (cos), STEADY (lunge_aim: aim cos
    // steadyAim, the lock held steadyLockHold against a switch, a lunge only at the locked
    // enemy in sight) and the web ball's RB lead on every shot (ball_lead ALWAYS).
    float lungeFrom = 1.3f, lungeSpeed = 2.6f, punchForward = .35f;
    bool lungeSteady = false, ballLeadAlways = false;
    // Update 38.2 (run381/DIAGNOSIS.md); these defaults are 38.1's (the game_xr sets 38.2's):
    //   punchTargeted (PUNCH MODE TARGETED): a hand attack only with an enemy within
    //     targetReach of the head or the lock held, thrown toward him (forward share at least
    //     targetForward), never with the hologram or mirror open (Frame::menuOpen) or the hand
    //     in the hologram zone (Hand::zoned). 38.1's gesture punch needed no enemy at all: an
    //     arm waved fast forward sent X (run381: kicks with nobody near).
    //   meleePunchOnly (MELEE ANIM PUNCH ONLY): each blow one spaced X tap (combat38::MeleeSpacer)
    //     with the left stick neutral while it is down (Out::stickNeutral).
    //   gadgetEasy (GADGET FEEL EASY): the Sonic Burst first and looser (no fallback to a
    //     single-hand gadget for a two-trigger press); Upshot without the aim-low start;
    //     Ricochet with the grip in either order; the Grabber on a pull toward the body (38.1
    //     passed SpidyVR's yank verdict, so only that strict detector could fire it: 0 in
    //     run381); the stronger of Upshot / Grabber when both match.
    //   assistSmart (BALL AIM SMART): combat38::smartTarget, sticky to the last pick.
    bool punchTargeted = false, meleePunchOnly = false, gadgetEasy = false, assistSmart = false;
    // Version 0.0.4 (rep0030, "i punch fast it needs to register every single hit"):
    //   meleeEveryHit (MELEE ANIM EVERY HIT, the default; meleePunchOnly is SPACED, 0.0.3's):
    //     each blow its own short X tap at once (combat38::PunchTaps), the stick neutral.
    //   quickRearm: a fist re-arms as soon as its swing ends (under the swing speed) or it
    //     pulls back toward the head, not only under rearmSpeed or after rearmSeconds (rep0030:
    //     2729 frames of fast jabs refused as "not re-armed").
    bool meleeEveryHit = false, quickRearm = false;
    // Version 0.0.4 CLOSE JABS: with TARGETED's enemy in reach, a fist at 0.85 of punch_speed
    // is a punch (rep0030: 170 forward swings peaked 1.4-2.4 m/s, under 2.4; the replay at
    // 2.0 m/s: 132 -> 148+ blows). Never with no enemy in reach, so waving still sends nothing.
    bool closeJabs = false;
    static constexpr float closeShare = .85f;
    static constexpr float pullBack = .8f; // m/s back toward the head: a jab's return
    // Version 0.0.5 PUNCH SENSITIVITY (combat38::punchFeelOf, applyPunchFeel): the forward share's
    // factor (under 1 a wider cone), the speed back toward the head that re-arms a fist (a partial
    // pull-back; 0.0.4's pullBack), and the least time between one hand's blows.
    float forwardK = 1.f, rearmBack = pullBack, minBlowGap = 0.f;
    // Version 0.0.3 probe: a held-off swing with an enemy past targetReach but within
    // heldOffFarReach counts as heldOffFar (rep0023 held off 90 swings; the replay shows an
    // enemy 3-4.5 m ahead already gets the far lunge, so the 3 m reach stays).
    static constexpr float targetReach = 3.f, targetForward = .45f, heldOffFarReach = 12.f;
    // Update 38.3 (run382/DIAGNOSIS.md); these defaults are 38.2's (the game_xr sets 38.3's):
    //   lookQuick (LOOK LOCK QUICK): the lock moves to the enemy you look straight at after
    //     quickDwell (with the kept one quickMargin farther off the line, or past quickAside),
    //     and STEADY's 0.5 s hold stands aside. 38.2 moved it only with the kept enemy over 30
    //     degrees off the look for 0.3 s plus the 0.5 s hold: in a crowd it never moved.
    //   strikeSteer (STRIKE STEER TO LOCK): a web strike's Y waits assistLead with the left stick
    //     toward its enemy (CANDIDATE: the game's strike reads the stick, as the ball assist).
    bool lookQuick = false, strikeSteer = false;
    // Update 38.5 (run384/DIAGNOSIS.md; default false = 38.4's, the game_xr sets it):
    //   farLunge: a far throw zips on the first try - the lock and the lunge reach zipFar (20 m,
    //     was 10), the marked enemy takes a throw within farAim of him (by its motion or by the
    //     arm's own line, WebStrikeConfig::reachAim; STEADY's 0.7 was 45 degrees), a hand re-arms
    //     after farRearm even while it keeps moving (a swinging arm never slowed under 1.2 m/s),
    //     is free farFree after its trigger or grip (was 0.35 s: mid-air hands come off a web),
    //     and with no lock yet the enemy the throw points at is taken.
    bool farLunge = false;
    static constexpr float farAim = .5f, farOtherAim = .6f, farRearm = .45f, farFree = .15f;
    // Update 38.5 ONE BALL A PRESS: each web-ball press is its own RB pulse, queued ballGap
    // apart (38.4 shared one pulse: two presses close together merged into one ball, and the
    // game chose the hand); Out::ballStart names the hand whose pulse begins (the wrist muzzle).
    static constexpr float ballGap = .05f;
    static constexpr unsigned maxQueued = 4;
    static constexpr float quickDwell = .15f, quickAside = .906f, quickMargin = .14f, quickSquare = .985f;
    // EASY's thresholds (m/s, before looseness): up, pull toward the body, forward flick;
    // the Sonic pose's reach, gap and the two presses' window.
    static constexpr float easyUp = 1.0f, easyPull = 1.1f, easyFlick = 1.2f, easyBlastReach = .15f, easyBlastGap = .45f,
                           easyGrace = .20f, easyBallHold = .8f, easyBallWait = .35f, easyFormedWait = .55f;
    // Update 38.3: web_yank ON by default (the user: "if they're a toggle they should be on by
    // default"; run382 sent 0 yanks with it off). Its input is still a CANDIDATE (yank_slot).
    bool webYank = true;            // web_yank
    // Update 38.5: Y HOLD (was RB HOLD: run384's yank gesture fired 0 times on it, and the game's
    // published yank input is hold Triangle). Plan B: yank_slot 5 (RB HOLD).
    uint32_t yankSlot = yHold;      // yank_slot
    // Forgiving aim. The game picks its own target for each attack (CONFIRMED: its
    // AimAssistTargetSelection classes exist; Spider-Man 2's bot list is NOT evidenced, so
    // no enemy is ever read). CANDIDATE: the game sends an attack toward the left stick's
    // direction, so a shot, gadget or punch pushes the stick toward where the wrist points.
    bool steer = false;             // aim_steer; Update 37.2: OFF (it overrode the stick, run371)
    float aimCone = 18.f;           // aim_cone (deg): within it of your look, the game's own pick
    float lockStick = 12.f;         // lock_stick (deg): the aim holds until the wrist turns this far
    float loose = 1.3f;             // looseness (1 as specified, larger = easier gestures)
    static constexpr float lockSeconds = 1.5f, steerSeconds = .15f;
    static constexpr float baseGrace = .12f;   // both triggers within this: the burst's, not two shots
    // Update 38: the punch's X held 0.18 s (was 0.14: the game missed short ones); a hand
    // re-arms under rearmSpeed or rearmSeconds after its hit (SpidyVR's punch.cpp rule; it
    // replaced 37.2's 0.30 s cooldown); the assist's stick leads its RB by assistLead so the
    // game reads the aim first; balls at most every ballInterval (Shooter's interval);
    // a SPIDYVR MOVE lunge's X when it lands.
    static constexpr float tapPulse = .08f, comboLead = .05f, comboHold = .20f, punchPulse = .18f,
                           gripPress = .5f, assistLead = .03f, assistHold = .20f,
                           rearmSpeed = PunchConfig{}.rearmSpeed, rearmSeconds = PunchConfig{}.rearmTime,
                           ballInterval = .08f, fistAhead = .10f;
    // Update 37.2: the speed's sanity (a sample needs this much time; faster than glitch is a
    // tracking jump, the speed restarts), the Sonic Burst's cooldown and pose height, the
    // zip's and the yank's pulses.
    static constexpr float minSample = .004f, maxGap = .1f, glitchSpeed = 15.f, sonicCooldown = 1.f,
                           blastBelow = .65f, blastAbove = .25f, zipPulse = .12f, yankHold = .5f, yHoldSpan = .6f;
    static constexpr uint32_t minTapMs = 100, maxTapMs = 800, minGestureMs = 300, maxGestureMs = 3000;
};
// Update 38.5: a far lunge's strike, the zip strike's reach (Config::farLunge), for this machine
// and for FLY MODE's lunge in the swing bridge (combat_spidy::Lunger::configure): one reach.
inline WebStrikeConfig farStrike(WebStrikeConfig sc) {
    sc.maxDistance = combat38::zipFar;
    sc.markedAim = Config::farAim;
    sc.aim = std::min(sc.aim, Config::farOtherAim);
    sc.reachAim = true;
    sc.rearmSeconds = Config::farRearm;
    sc.freeSeconds = Config::farFree;
    return sc;
}
// Version 0.0.5 PUNCH SENSITIVITY: the value's feel on a config already set (PUNCH SPEED, PUNCH ANGLE).
inline void applyPunchFeel(Config& c, uint32_t value) {
    const combat38::PunchFeel f = combat38::punchFeelOf(value);
    c.punchSpeed *= f.speedK;
    c.punchReach *= f.reachK;
    c.forwardK = f.forwardK;
    c.rearmBack = f.backSpeed;
    c.minBlowGap = f.minGap;
}
struct Hand {
    bool tracked{}, free{};   // free: the trigger is this module's (see the rule above)
    // Update 38 (SpidyVR's rule): its own web holds something or carries the player, or it
    // holds a grab, the wall or the hologram: no punch from it.
    bool busy{};
    bool zoned{};             // Update 38.2: the hand is in the hologram's zone (no attack from it)
    bool onCharacter{};       // Update 38: the wrist's web ray is on a character (AIMED zip)
    float button{};           // the web-ball button (the trigger), 0..1
    float grip{};             // the other button (the grip), 0..1: Ricochet's
    Vec3 position{};          // tracking space, metres
    Vec3 palm{};              // unit, the palm's outward normal (zero: unknown)
    Vec3 aim{};               // unit, the web-start ray (the grip's forward)
};
struct Frame {
    float seconds{};          // since the previous frame
    bool play{};              // VR play, nothing else holding the controls
    bool webAttached{};       // Update 37.2: any swing web attached (no gadget then)
    float enemyMetres = 1e9f; // Update 37.2: the nearest enemy (WORLD LOG, read-only; 1e9 unknown)
    // Update 38: the enemies' feet, tracking space (world_trace at 10 Hz; fresh ones only).
    std::array<Vec3, combat38::maxEnemies> enemies{};
    uint32_t enemyCount{};
    bool airborne{};          // Update 38: in the air or on a swing (combat allowed there now)
    std::array<Hand, 2> hands{};
    Vec3 head{}, forward{};   // head position and its look, tracking space
    // Update 38 (SpidyVR fold-in): each enemy's registry id (0: unknown) and what the latest
    // sight ray to him found (combat_spidy::Sight); the player's feet, tracking space (zero
    // with hasFeet off: 1.6 m under the head).
    std::array<uint64_t, combat38::maxEnemies> enemyIds{};
    std::array<uint8_t, combat38::maxEnemies> enemySight{};
    Vec3 feet{};
    bool hasFeet{};
    // LUNGE SPIDYVR MOVE: the swing bridge flies the lunges (bridgeLunge: its ray module has
    // them); lungeLanded: one landed since the last frame (the game's X goes out).
    bool bridgeLunge{}, lungeLanded{};
    bool menuOpen{};          // Update 38.2: the hologram or the mirror is open (TARGETED: no attack)
};
struct Out {
    uint16_t buttons{};       // for the virtual pad this frame
    uint32_t fired[2]{};      // the Gadget a hand fired this frame (sonicBurst: both)
    bool punch[2]{};
    bool holdsGrip[2]{};      // the swing must not take this hand's grip as a web
    float speed[2]{};
    bool steering{};          // the left stick is the aim's this frame
    float stickX{}, stickY{}; // -1..1, the game's left stick (x right, y forward)
    float aimAngle{};         // degrees off the look, of the aim in force
    bool latched{};
    bool zip{};               // Update 37.2: this frame's hit went out as the zip strike (Y)
    // Update 38
    bool contact[2]{};        // the hit was a hand touching an enemy (not the fast gesture)
    bool assisted{};          // a web ball bent toward an enemy this frame
    float assistDegrees = 180.f;
    Vec3 ballDirection[2]{};  // a web ball's flight (assisted or the wrist's), for the glob
    float ballMetres[2]{};
    uint32_t reject[2]{};     // why a punch candidate was not sent (combat38::Reject; sent when none)
    bool candidate[2]{};
    // SpidyVR fold-in: a lunge opened this frame (hand -1 none), at which enemy, how far.
    int lungeHand = -1;
    uint64_t lungeTarget{};
    float lungeMetres{};
    uint64_t lockTarget{};    // the head-look lock's enemy (0 none)
    bool yanked[2]{};
    bool stickNeutral{};      // Update 38.2 PUNCH ONLY: hold the left stick at rest (an X tap is down)
    bool recognised[2]{};     // Update 38.2: a gadget recognised this frame (EASY: the buzz)
    bool strikeSteered{};     // Update 38.3: the stick is STRIKE STEER's this frame
    // Update 38.5: a web ball's RB pulse begins this frame for this hand (one per press), and
    // the hand whose ball pulse is down now (-1 none).
    bool ballStart[2]{};
    int ballHand = -1;
    // Version 0.0.2 BALL LOOK OURS: this hand's press fires our own ball this frame (no RB).
    bool ballOurs[2]{};
};
inline Vec3 level(Vec3 v) {
    Vec3 h{v.x, 0, v.z};
    const float l = length(h);
    return l > 1e-4f ? h / l : Vec3{};
}
// Looseness scales every gesture threshold the forgiving way.
inline float looseness(const Config& c) { return std::isfinite(c.loose) ? std::clamp(c.loose, .5f, 2.f) : 1.f; }
inline float pressAt(const Config& c) { return std::clamp(.65f / looseness(c), .3f, .9f); }
inline float graceOf(const Config& c) { return Config::baseGrace * looseness(c); }
// The Sonic Burst's pose: both hands out in front, close together side by side.
inline bool blastPose(const Frame& f, const Config& c) {
    const float l = looseness(c);
    const Vec3 ahead = level(f.forward);
    if (length(ahead) < .5f)
        return false;
    for (const auto& h : f.hands)
        if (!h.tracked || !finite(h.position) || dot(h.position - f.head, ahead) < c.blastReach / l)
            return false;
    if (length(f.hands[0].position - f.hands[1].position) > c.blastGap * l)
        return false;
    // Update 37.2: both hands about chest height (not a panicked mash at the hips or overhead).
    for (const auto& h : f.hands)
        if (h.position.y < f.head.y - Config::blastBelow || h.position.y > f.head.y + Config::blastAbove)
            return false;
    if (c.blastPalms)
        for (const auto& h : f.hands)
            if (!(length(h.palm) > .5f) || dot(h.palm, ahead) < .3f / l)
                return false;
    return true;
}
// The single-hand gesture a held trigger makes now, by priority; none while none.
// yanked (Update 38): SpidyVR's yank detector's verdict this frame (combat_spidy::Yank); -1
// for 37.2's velocity rule instead (toward the body at yank_speed).
inline Gadget gesture(const Hand& h, Vec3 velocity, Vec3 head, bool aimedLow, bool gripLater, const Config& c,
                      int yanked = -1) {
    const float l = looseness(c);
    if (!finite(velocity))
        return none;
    // Update 37.2: WEB YANK, the grip with the trigger and a sharp pull back (before Ricochet's
    // forward flick: the two go opposite ways, so they never both match).
    if (c.webYank && c.yankSlot && gripLater && h.grip >= Config::gripPress) {
        const Vec3 toward = head - h.position;
        if (yanked >= 0 ? yanked > 0
                        : length(toward) > .05f && dot(velocity, normalized(toward)) >= 1.5f * c.yankSpeed / l)
            return webYank;
    }
    if (c.ricochetSlot && gripLater && h.grip >= Config::gripPress && length(h.aim) > .5f &&
        dot(velocity, normalized(h.aim)) >= c.flickSpeed / l)
        return ricochetWeb;
    if (c.upshotSlot && aimedLow && velocity.y >= c.upSpeed / l && velocity.y >= .6f * length(velocity))
        return upshot;
    const Vec3 toBody = head - h.position;
    if (c.grabberSlot &&
        (yanked >= 0 ? yanked > 0 : length(toBody) > .05f && dot(velocity, normalized(toBody)) >= c.yankSpeed / l))
        return webGrabber;
    return none;
}
// Update 38.2 GADGET FEEL EASY's single-hand gesture: Ricochet (the grip held, in either
// order, and a forward flick), then the stronger of Upshot (up) and the Grabber (toward the
// body), each looser than 38.1's and with no aim-low start.
inline Gadget easyGesture(const Hand& h, Vec3 velocity, Vec3 head, const Config& c, int yanked) {
    const float l = looseness(c);
    if (!finite(velocity))
        return none;
    // Update 38.3: WEB YANK in EASY, the grip held with the trigger and a pull toward the body
    // (Ricochet's flick goes the other way; the Grabber is the same pull without the grip).
    if (c.webYank && c.yankSlot && h.grip >= Config::gripPress) {
        const Vec3 flat = level(head - h.position);
        const float back = length(flat) > .5f ? dot(velocity, flat) : 0.f;
        if (yanked > 0 || (back >= Config::easyPull / l && back >= .5f * length(velocity)))
            return webYank;
    }
    if (c.ricochetSlot && h.grip >= Config::gripPress) {
        const Vec3 along = length(h.aim) > .5f ? normalized(h.aim) : Vec3{};
        if (dot(velocity, along) >= Config::easyFlick / l)
            return ricochetWeb;
    }
    const float speed = length(velocity);
    const float up = velocity.y;
    const Vec3 toBody = head - h.position;
    const Vec3 flatBody = level(toBody);
    const float pull = length(flatBody) > .5f ? dot(velocity, flatBody) : 0.f;
    const bool upOk = c.upshotSlot && up >= Config::easyUp / l && up >= .55f * speed;
    // Update 38.2: a pull is the Grabber only when it beats the rise (an up-flick swings the
    // hand back toward the head too: run372 112.8 s, 3.6 m/s up, was a Grabber).
    const bool pullOk = c.grabberSlot && up < std::max(pull, Config::easyUp / l) &&
                        (yanked > 0 || (pull >= Config::easyPull / l && pull >= .5f * speed));
    if (upOk && pullOk)
        return up >= pull ? upshot : webGrabber;
    return upOk ? upshot : pullOk ? webGrabber : none;
}
// EASY's Sonic pose: both hands at least easyBlastReach in front, within easyBlastGap of
// each other, between the hips and over the head.
inline bool easyBlastPose(const Frame& f) {
    const Vec3 ahead = level(f.forward);
    if (length(ahead) < .5f)
        return false;
    for (const auto& h : f.hands)
        if (!h.tracked || !finite(h.position) || dot(h.position - f.head, ahead) < Config::easyBlastReach ||
            h.position.y < f.head.y - .85f || h.position.y > f.head.y + .35f)
            return false;
    return length(f.hands[0].position - f.hands[1].position) <= Config::easyBlastGap;
}
inline bool aimsLow(Vec3 aim, const Config& c) {
    if (!(length(aim) > .5f))
        return false;
    const float below = std::asin(std::clamp(-normalized(aim).y, -1.f, 1.f)) * 57.2957795f;
    return below >= std::clamp(c.lowDegrees, 0.f, 80.f) / looseness(c);
}

class Machine {
  public:
    static constexpr float release = .25f;
    Out update(const Frame& f, const Config& c) {
        Out out;
        const float press = pressAt(c), grace = c.gadgetEasy ? std::max(graceOf(c), Config::easyGrace) : graceOf(c);
        const float dt = std::isfinite(f.seconds) ? std::clamp(f.seconds, 0.f, .1f) : 0.f;
        now_ += dt;
        const float tapSeconds = static_cast<float>(std::clamp(c.tapMs, Config::minTapMs, Config::maxTapMs)) / 1000.f;
        const float gestureSeconds =
            static_cast<float>(std::clamp(c.gestureMs, Config::minGestureMs, Config::maxGestureMs)) / 1000.f;
        // Speeds first (also for the report).
        for (unsigned i = 0; i < 2; ++i) {
            const auto& h = f.hands[i];
            auto& s = hands_[i];
            // Update 37.2: a sample spans at least minSample (a near-zero frame time made
            // 57,000 m/s in run371); a gap or a jump faster than glitchSpeed restarts it.
            // Update 38 (SpidyVR): the hand's motion relative to the head, the arm's own (a
            // swing or a fall moves both alike and is no punch).
            const Vec3 relative = h.position - f.head;
            const bool valid = h.tracked && finite(h.position) && finite(relative);
            s.age += dt;
            s.jumped = false;
            if (valid && s.seen) {
                if (s.age > Config::maxGap) {
                    s.velocity = {};
                    s.last = relative;
                    s.age = 0;
                } else if (s.age >= Config::minSample) {
                    const Vec3 v = (relative - s.last) / s.age;
                    if (!finite(v) || length(v) > Config::glitchSpeed) {
                        s.velocity = {};
                        s.jumped = true;
                        ++jumps_;
                    } else {
                        s.velocity = s.velocity + (v - s.velocity) * .6f;
                    }
                    s.last = relative;
                    s.age = 0;
                }
            } else {
                s.velocity = {};
                s.last = relative;
                s.age = 0;
            }
            s.seen = valid;
            out.speed[i] = length(s.velocity);
        }
        if (!c.on || !f.play) {
            for (unsigned i = 0; i < 2; ++i) {
                auto& s = hands_[i];
                if (s.state != State::idle)
                    s.state = f.hands[i].button < release ? State::idle : State::spent;
            }
            pulseUntil_ = comboUntil_ = punchUntil_ = zipUntil_ = assistUntil_ = pulseFrom_ = 0;
            queued_ = 0;
            everyHit_.clear(); // Version 0.0.4: no waiting blow survives a pause
            ballLast_ = -1e3f;
            zipFrom_ = strikeUntil_ = 0;
            fists_.reset();
            strike_.cancel();
            for (unsigned i = 0; i < 2; ++i) {
                hands_[i].touching = false;
                hands_[i].yank.reset();
                // Update 38: a punch-like move while combat is gated, counted with its reason.
                if (c.on && c.punch && f.hands[i].tracked && out.speed[i] >= .6f * c.punchSpeed / looseness(c)) {
                    out.candidate[i] = true;
                    out.reject[i] = combat38::gated;
                    ++rejects_[combat38::gated];
                    noteSwing(hands_[i], combat38::gated, out.speed[i]);
                } else {
                    endSwing(hands_[i]);
                }
            }
            return out;
        }
        // Presses and releases.
        bool down[2]{};
        for (unsigned i = 0; i < 2; ++i) {
            const auto& h = f.hands[i];
            auto& s = hands_[i];
            const bool usable = h.tracked && h.free;
            // Update 38.5 ONE BALL PER PULL: a new pull while the last tap still waits out the
            // burst's grace (pulls 0.12 s apart) fires that tap's ball now, then takes the pull:
            // the same hand twice is never the burst, and no pull is dropped.
            if (s.state == State::tapPending && usable && h.button > press) {
                s.ballAt = now_;
                out.fired[i] = webShooter;
                ++fired_[webShooter];
                s.state = State::idle;
            }
            if (s.state == State::idle && usable && h.button > press) {
                s.state = State::held;
                s.since = now_;
                s.aimedLow = aimsLow(h.aim, c);
                s.gripAtPress = h.grip >= Config::gripPress; // a grip held first is the swing's
                s.ballFired = false;
                s.formed = false;
                s.aimAtPress = length(h.aim) > .5f ? normalized(h.aim) : Vec3{};
                s.yank.reset();
                s.paired = false;
            } else if ((s.state == State::held || s.state == State::spent) && h.button < release) {
                // (a tap waiting out the burst's grace stays pending: it must still fire)
                // Let go: a quick one is the web shooter's tap, once the grace passed.
                // Update 38 BALL FIRE PRESS: the ball went at the press already, or (let go
                // within the burst's grace) goes now that the grace is past.
                s.state = s.state == State::held && (c.ballOnPress ? !s.ballFired : now_ - s.since <= tapSeconds)
                              ? State::tapPending
                              : State::idle;
            } else if (!usable && (s.state == State::held || s.state == State::tapPending)) {
                s.state = State::spent; // a web caught or the button was claimed: nothing fires
            }
            down[i] = s.state == State::held;
        }
        // The Sonic Burst first: both pressed within the grace, in its pose. Update 37.2: never
        // with a web attached, and once a second at most.
        const bool gadgetsOk = !f.webAttached && !c.noTriggerGadgets;
        if (c.sonicSlot && gadgetsOk && down[0] && down[1] && std::abs(hands_[0].since - hands_[1].since) <= grace) {
            const bool pose = c.gadgetEasy ? easyBlastPose(f) : blastPose(f, c);
            if (pose && now_ - sonicAt_ >= Config::sonicCooldown) {
                sonicAt_ = now_;
                out.fired[0] = out.fired[1] = sonicBurst;
                out.recognised[0] = out.recognised[1] = true;
                ++fired_[sonicBurst];
                ++recognised_;
                startCombo(c.sonicSlot);
                for (auto& s : hands_)
                    s.state = State::spent;
            } else if (now_ - std::max(hands_[0].since, hands_[1].since) < .01f) {
                ++blastMissed_;
            }
            // EASY: a two-trigger press is the burst's or two balls, never a single-hand gadget.
            if (c.gadgetEasy)
                hands_[0].paired = hands_[1].paired = true;
        }
        for (unsigned i = 0; i < 2; ++i) {
            auto& s = hands_[i];
            const auto& h = f.hands[i];
            const float age = now_ - s.since;
            // The other hand may still make it a burst: wait out the grace.
            const bool otherMayJoin = c.sonicSlot && hands_[1 - i].state == State::idle && age < grace;
            if (s.state == State::tapPending && !otherMayJoin) {
                s.ballAt = now_;
                out.fired[i] = webShooter;
                ++fired_[webShooter];
                s.state = State::idle; // its RB queued below (one press, one pulse)
            } else if (s.state == State::held && (!otherMayJoin || c.gadgetEasy)) {
                // Update 38 BALL FIRE PRESS (SpidyVR's shooter): the ball at the press, once
                // the burst's grace is past, at most every ballInterval.
                // Update 38.2 EASY: the gesture is read during the burst's grace too (the
                // 0.2 s grace ate the fast up-flicks: run38 207.8 s, 3.1 m/s up, no Upshot);
                // only the ball still waits it out.
                // Update 38.2 EASY: the ball also waits while the hand is already moving like a
                // gadget (faster than easyBallHold, at most easyBallWait after the press), so an
                // Upshot / Grabber move is the gadget, not a ball first (the 38.1 complaint).
                // Update 38.3 (run382: 8 of 14 moves already under way fired the ball at 0.2 s,
                // the gadget at 0.25-0.51 s): once the hand moved like a gadget in the first
                // easyBallWait, the ball waits up to easyFormedWait for its gesture.
                const bool fastNow = length(s.velocity) >= Config::easyBallHold;
                s.formed = s.formed || (c.gadgetEasy && age < Config::easyBallWait && fastNow);
                const bool gadgetForming = c.gadgetEasy && ((age < Config::easyBallWait && fastNow) ||
                                                            (s.formed && age < Config::easyFormedWait));
                // Version 0.0.4: OURS sends no RB, so its ball goes at the press as SpidyVR's
                // does (the gadget can still follow on the same pull); rep0030's waits lost balls.
                if ((c.ballOurs || (!otherMayJoin && !gadgetForming)) && c.ballOnPress && !s.ballFired &&
                    now_ - s.ballAt >= Config::ballInterval) {
                    s.ballFired = true;
                    s.ballAt = now_;
                    out.fired[i] = webShooter;
                    ++fired_[webShooter];
                }
                // SpidyVR's yank: back along the shot, head-relative.
                const bool yanked = length(s.aimAtPress) > .5f &&
                                    s.yank.update(dt, h.position - f.head, s.aimAtPress, looseness(c));
                out.yanked[i] = yanked && !c.noTriggerGadgets;
                yanks_ += out.yanked[i];
                if (age > gestureSeconds) {
                    s.state = State::spent; // a hold is unassigned: it does nothing
                } else if (const auto g = !gadgetsOk || out.fired[i] == webShooter ? none
                                          : c.gadgetEasy ? (s.paired ? none : easyGesture(h, s.velocity, f.head, c, yanked ? 1 : 0))
                                                         : gesture(h, s.velocity, f.head, s.aimedLow, !s.gripAtPress, c,
                                                                   yanked ? 1 : 0);
                           g != none) {
                    out.fired[i] = g;
                    out.recognised[i] = true;
                    ++recognised_;
                    ++fired_[g];
                    startCombo(g == webGrabber ? c.grabberSlot
                               : g == upshot   ? c.upshotSlot
                               : g == webYank  ? c.yankSlot
                                               : c.ricochetSlot);
                    s.state = State::spent;
                }
            }
            if (s.state == State::spent && h.button < release)
                s.state = State::idle;
            // A trigger held for a gesture keeps its grip from the swing (Ricochet's grip).
            out.holdsGrip[i] = (s.state == State::held || s.state == State::spent) && (!s.gripAtPress || (c.gadgetEasy && !f.webAttached));
        }
        // Update 38 (SpidyVR fold-in): the enemies as SpidyVR's types take them, the sight
        // world over the sight rays, the head-look lock.
        const uint32_t count = std::min<uint32_t>(f.enemyCount, combat38::maxEnemies);
        std::array<PunchTarget, combat38::maxEnemies> punchTargets{};
        std::array<FightTarget, combat38::maxEnemies> fightTargets{};
        std::array<combat_spidy::SightWorld::Entry, combat38::maxEnemies> sights{};
        for (uint32_t e = 0; e < count; ++e) {
            const uint64_t id = f.enemyIds[e] ? f.enemyIds[e] : 0x38000000ull + e; // unknown: by place
            punchTargets[e] = combat_spidy::punchTarget(id, f.enemies[e]);
            fightTargets[e] = combat_spidy::fightTarget(id, f.enemies[e]);
            sights[e] = {fightTargets[e].middle(), static_cast<combat_spidy::Sight>(f.enemySight[e])};
        }
        const std::span<const PunchTarget> punchList{punchTargets.data(), count};
        const std::span<const FightTarget> fightList{fightTargets.data(), count};
        const combat_spidy::SightWorld sightWorld{std::span<const combat_spidy::SightWorld::Entry>{sights.data(), count}};
        // Update 38.3 LOOK LOCK: QUICK's or 38.2's rule (the lock and its mark stay).
        {
            LockConfig lc{};
            if (c.lookQuick) {
                lc.dwell = Config::quickDwell;
                lc.aside = Config::quickAside;
                lc.margin = Config::quickMargin;
                lc.square = Config::quickSquare; // the head within 10 degrees of the new one
            }
            if (c.farLunge)
                lc.reach = combat38::zipFar; // Update 38.5: the lock as far as the lunge
            if (lc.dwell != lock_.config().dwell || lc.margin != lock_.config().margin ||
                lc.reach != lock_.config().reach)
                lock_.configure(lc);
        }
        const uint64_t raw = lock_.update(dt, f.head, f.forward, fightList);
        // Update 38.1 STEADY: a switch from a lock still in the fight waits steadyLockHold
        // (one back within it is counted as held); a first lock or a lost one is at once.
        uint64_t locked = raw;
        if (c.lungeSteady && !c.lookQuick && raw != lockedLast_ && lockedLast_) {
            bool present = false;
            for (const auto& t : fightList)
                present |= t.id == lockedLast_;
            if (present && (switchFor_ += dt) < combat38::steadyLockHold) {
                locked = lockedLast_;
                switching_ = true;
            } else {
                switchFor_ = 0;
                switching_ = false;
            }
        } else {
            lockHeld_ += switching_ && raw == lockedLast_;
            switchFor_ = 0;
            switching_ = false;
        }
        lockChanges_ += locked != lockedLast_;
        lookSwitches_ += locked && lockedLast_ && locked != lockedLast_; // Update 38.3: one enemy to another
        lockedLast_ = locked;
        out.lockTarget = locked;
        const FightTarget* lockedTarget{};
        for (const auto& t : fightList)
            if (locked && t.id == locked)
                lockedTarget = &t;
        const Vec3 feet = f.hasFeet && finite(f.feet) ? f.feet : f.head - Vec3{0, 1.6f, 0};
        // SpidyVR's punches: the 8 cm fist (10 cm out along the hand) swept into each body
        // capsule; its motion relative to the head. A tracking jump this frame is no motion.
        std::array<PunchHand, 2> fistIn{};
        const bool gestures = c.punchMode != 2, contacts = c.punchMode != 1;
        for (unsigned i = 0; i < 2; ++i) {
            const auto& h = f.hands[i];
            const Vec3 fist = h.position + (length(h.aim) > .5f ? normalized(h.aim) * Config::fistAhead : Vec3{});
            fistIn[i] = {h.tracked && !hands_[i].jumped && finite(fist), fist, h.position - f.head,
                         !c.punch || !contacts};
        }
        events_.clear();
        fists_.update(dt, fistIn, punchList, events_);
        bool struck[2]{};
        for (const auto& e : events_)
            if (e.hand >= 0 && e.hand < 2)
                struck[e.hand] = true;
        // Re-arm (SpidyVR's rule, for the gesture too): slow under rearmSpeed, or rearmSeconds.
        for (unsigned i = 0; i < 2; ++i) {
            auto& s = hands_[i];
            // Version 0.0.5: never above the candidate speed, so one slow swing (PUNCH SENSITIVITY
            // MAX, needing under 1.2 m/s) cannot re-arm and land again on every frame.
            const float rearmUnder = std::min(Config::rearmSpeed, .6f * c.punchSpeed / looseness(c));
            if (!s.armed && (out.speed[i] < rearmUnder || now_ - s.punchedAt > Config::rearmSeconds))
                s.armed = true;
            // Version 0.0.4 quickRearm: the swing is over, or the fist comes back.
            if (!s.armed && c.quickRearm && now_ > s.punchedAt) {
                const Vec3 out0 = level(f.hands[i].position - f.head);
                const bool back = length(out0) > .05f && dot(s.velocity, normalized(out0)) <= -c.rearmBack;
                if ((back || out.speed[i] < .6f * c.punchSpeed / looseness(c)) && now_ - s.punchedAt >= c.minBlowGap)
                    s.armed = true;
            }
            // Update 38.1: a swing ends once the hand slows under the candidate speed.
            if (out.speed[i] < .6f * c.punchSpeed / looseness(c) && !(c.punch && struck[i]))
                endSwing(s);
        }
        // The lunge (ZIP MODE SPIDYVR): SpidyVR's WebStrike::open with the lock as its wish.
        // LUNGE GAME BUTTON: the game's own web strike (Y) at once; SPIDYVR MOVE: the bridge
        // flies its own (Frame::bridgeLunge) and the game's X goes out when it lands.
        const bool spidyZip = c.zipStrike && c.punch && c.zipMode == combat38::zipSpidy && count > 0;
        const bool bridgeFlies = c.lungeMode == combat38::lungeSpidyMove && f.bridgeLunge;
        // Update 38.1: the lunge's distance, speed and (STEADY) aim from the COMBAT settings,
        // taken while no strike is in flight.
        {
            WebStrikeConfig sc{};
            sc.minDistance = std::isfinite(c.lungeFrom) ? std::clamp(c.lungeFrom, .5f, 8.f) : sc.minDistance;
            sc.minSpeed = std::isfinite(c.lungeSpeed) ? std::clamp(c.lungeSpeed, 1.f, 5.f) : sc.minSpeed;
            if (c.lungeSteady)
                sc.aim = sc.markedAim = combat38::steadyAim;
            if (c.farLunge)
                sc = farStrike(sc);
            if ((sc.minDistance != strikeConfig_.minDistance || sc.minSpeed != strikeConfig_.minSpeed ||
                 sc.aim != strikeConfig_.aim || sc.markedAim != strikeConfig_.markedAim ||
                 c.farLunge != farApplied_) &&
                strike_.phase() == WebStrike::Phase::Idle) {
                strikeConfig_ = sc;
                strike_ = WebStrike(sc);
                farApplied_ = c.farLunge;
            }
        }
        if (spidyZip && !bridgeFlies) {
            std::array<FightFist, 2> fists{};
            for (unsigned i = 0; i < 2; ++i) {
                const auto& h = f.hands[i];
                const auto& s = hands_[i];
                fists[i] = {fistIn[i].tracked, fistIn[i].fist, s.velocity,
                            !h.busy && h.button < release && s.state == State::idle, 0, 0};
            }
            StrikeWish wish;
            wish.target = locked;
            const bool looking = finite(f.forward) && length(f.forward) > .5f;
            auto strikeable = looking && !locked && !c.farLunge ? std::span<const FightTarget>{} : fightList;
            // STEADY: only the locked enemy, and only while his sight ray is not blocked.
            // Update 38.5 farLunge: with no lock yet, any enemy in sight the throw points at.
            std::array<FightTarget, combat38::maxEnemies> seen{};
            uint32_t seenCount = 0;
            if (c.lungeSteady && c.farLunge && !locked) {
                for (uint32_t e = 0; e < count; ++e)
                    if (f.enemySight[e] != combat_spidy::sightBlocked)
                        seen[seenCount++] = fightTargets[e];
                strikeable = std::span<const FightTarget>{seen.data(), seenCount};
            } else if (c.lungeSteady) {
                strikeable = {};
                for (uint32_t e = 0; e < count; ++e)
                    if (locked && fightTargets[e].id == locked && f.enemySight[e] != combat_spidy::sightBlocked)
                        strikeable = std::span<const FightTarget>{&fightTargets[e], 1};
            }
            const uint64_t unknownBefore = sightWorld.unknownAsked;
            if (const auto opened = strike_.open(dt, fists, f.head, feet, strikeable, sightWorld, wish)) {
                strike_.cancel(); // the game's move (or the bridge's), never this one's
                const unsigned i = static_cast<unsigned>(opened->hand);
                auto& s = hands_[i];
                out.lungeHand = opened->hand;
                out.lungeTarget = opened->target;
                out.lungeMetres = opened->distance;
                out.zip = true;
                out.punch[i] = true;
                out.candidate[i] = true;
                out.reject[i] = combat38::sent;
                ++rejects_[combat38::sent];
                noteSwing(s, combat38::sent, out.speed[i]);
                startZip(f, c, opened->target, fightList);
                ++fired_[zipStrike];
                ++lunges_;
                ++punches_[i];
                airPunches_ += f.airborne ? 1u : 0u;
                sightUnknown_ += sightWorld.unknownAsked > unknownBefore;
                s.punchedAt = now_;
                s.armed = false;
                fists_.spend(opened->hand);
                lock_.take(opened->target);
            }
        }
        // SPIDYVR MOVE: the bridge's lunge landed - the game's X now (its own blow).
        if (c.punch && f.lungeLanded && bridgeFlies) {
            punchUntil_ = now_ + Config::punchPulse;
            meleeWant_ = true;
            ++lungesLanded_;
        }
        // Hand hits: a fast, forward, extended hand with its trigger let go (GESTURE), or a
        // fist swept into an enemy's body (CONTACT, SpidyVR's); both by default. A candidate
        // (fast enough to look like a punch, or a hit) that is not sent counts its reason.
        if (c.punch) {
            const float l = looseness(c);
            // Toward the locked enemy when there is one (SpidyVR's punches go to him).
            const Vec3 toLocked = lockedTarget ? level(lockedTarget->middle() - f.head) : Vec3{};
            Vec3 ahead = length(toLocked) > .5f ? toLocked : level(f.forward);
            // Update 38.2 TARGETED: the nearest enemy within targetReach (flat, from the head),
            // or the lock; none: no attack at all.
            bool target = lockedTarget != nullptr, enemyBeyond = false;
            if (c.punchTargeted && !lockedTarget) {
                float best = Config::targetReach;
                for (uint32_t e = 0; e < count; ++e) {
                    const Vec3 to = fightTargets[e].middle() - f.head;
                    const float d = length(Vec3{to.x, 0, to.z});
                    enemyBeyond = enemyBeyond || (finite(to) && d > best && d <= Config::heldOffFarReach);
                    if (finite(to) && d <= best) {
                        best = d;
                        target = true;
                        ahead = level(to);
                    }
                }
            }
            const bool targetedBlock = c.punchTargeted && (!target || f.menuOpen);
            // SpidyVR: a fist thrown at the locked enemy out of reach (1.3-10 m from the chest)
            // is the lunge's, not a gesture's blow in the air (ZIP MODE SPIDYVR; the bridge's
            // or the game's Y). It counts as too slow until it is fast enough to lunge.
            // Update 38.1: from strikeConfig_.minDistance (LUNGE FROM, 2.5 m); STEADY: only while
            // the lunge could take him (in sight), else the swing is a punch.
            bool lungeOwns{}, heldOff{};
            if (spidyZip && lockedTarget) {
                const WebStrikeConfig& sc = strikeConfig_;
                const float away = length(lockedTarget->middle() - (feet + Vec3{0, sc.chest, 0}));
                lungeOwns = away >= sc.minDistance && away <= sc.maxDistance;
                if (lungeOwns && c.lungeSteady)
                    for (uint32_t e = 0; e < count; ++e)
                        if (fightTargets[e].id == locked && f.enemySight[e] == combat_spidy::sightBlocked) {
                            lungeOwns = false;
                            heldOff = true;
                        }
            }
            // Update 38.5 farLunge: with no lock yet, the nearest enemy in sight decides - out of
            // reach (LUNGE FROM to 20 m) the throw is the lunge's, so a gesture's blow in the air
            // no longer spends the strike first (run382/384: the first far throws punched air).
            if (spidyZip && c.farLunge && !lockedTarget) {
                const WebStrikeConfig& sc = strikeConfig_;
                float nearest = 1e9f;
                for (uint32_t e = 0; e < count; ++e)
                    if (f.enemySight[e] != combat_spidy::sightBlocked)
                        nearest = std::min(nearest, length(fightTargets[e].middle() - (feet + Vec3{0, sc.chest, 0})));
                lungeOwns = nearest >= sc.minDistance && nearest <= sc.maxDistance;
            }
            for (unsigned i = 0; i < 2; ++i) {
                if (out.lungeHand == static_cast<int>(i))
                    continue;
                const auto& h = f.hands[i];
                auto& s = hands_[i];
                const float speed = out.speed[i];
                if (speed < .6f * c.punchSpeed / l)
                    s.targetedCounted = false;
                const bool enters = contacts && struck[i];
                s.touching = enters;
                const bool fast = gestures && speed >= .6f * c.punchSpeed / l;
                if (!fast && !enters)
                    continue;
                out.candidate[i] = true;
                if (heldOff && !s.heldOffCounted) { // once a swing
                    s.heldOffCounted = true;
                    ++lungeHeldOff_;
                }
                // Update 37.2: a forward-ish hand (hooks and uppercuts count: 0.35 of the speed
                // forward, was 0.5), and a far enemy makes it the zip strike.
                // Update 38.1: PUNCH ANGLE (0.2 by default; 38's 0.35).
                const float share = std::isfinite(c.punchForward) ? std::clamp(c.punchForward, 0.f, .9f) : .35f;
                const float need = (c.punchTargeted ? std::max(share, Config::targetForward) : share) *
                                   (std::isfinite(c.forwardK) ? std::clamp(c.forwardK, .3f, 1.5f) : 1.f);
                const bool forward = length(ahead) > .5f && dot(s.velocity, ahead) >= need * speed;
                const bool extended = dot(h.position - f.head, ahead) >= c.punchReach / l;
                const float needSpeed =
                    (c.closeJabs && c.punchTargeted && target ? Config::closeShare : 1.f) * c.punchSpeed / l;
                const bool gesture = fast && speed >= needSpeed && forward && extended && !lungeOwns;
                const bool contact = enters; // SpidyVR's own test: 2.2 m/s and inward, armed
                uint32_t why = combat38::sent;
                if (!h.tracked)
                    why = combat38::untracked;
                else if (h.busy)
                    why = combat38::notFree;
                else if (h.button >= release)
                    why = combat38::triggerHeld;
                else if (s.state != State::idle)
                    why = combat38::busy;
                else if (c.punchTargeted && (targetedBlock || h.zoned))
                    why = combat38::gated;
                else if (!contact && !s.armed)
                    why = combat38::cooldown;
                else if (!gesture && !contact)
                    why = !fast || speed < needSpeed || lungeOwns ? combat38::tooSlow
                          : !forward                         ? combat38::notForward
                                                             : combat38::notExtended;
                out.reject[i] = why;
                ++rejects_[why];
                // Version 0.0.3: only a swing that would have been a blow (a gesture or a contact)
                // counts as held off; a far swing the lunge owns (too slow for it, or its strike
                // busy) was never a punch (the rep0023 replay: 20 of those at 3.4 m inflated it).
                if (c.punchTargeted && why == combat38::gated && !s.targetedCounted && (gesture || contact)) {
                    s.targetedCounted = true; // once a swing
                    ++targetedHeldOff_;
                    heldOffFar_ += !target && enemyBeyond && !f.menuOpen ? 1u : 0u;
                }
                // Update 38.1: one count per swing (its reason at the swing's peak speed, unless
                // it sent a blow), when it ends; the per-frame counts above stay as 38's.
                noteSwing(s, why, speed);
                if (why != combat38::sent)
                    continue;
                out.punch[i] = true;
                out.contact[i] = contact;
                ++(out.contact[i] ? contactHits_ : gestureHits_);
                airPunches_ += f.airborne ? 1u : 0u;
                s.punchedAt = now_;
                s.armed = false;
                if (!contact)
                    fists_.spend(static_cast<int>(i)); // the gesture's blow re-arms the fist too
                strike_.spent(static_cast<int>(i));
                for (const auto& e : events_)
                    if (e.hand == static_cast<int>(i))
                        lock_.take(e.target);
                if (zipsNow(f, c, i)) {
                    out.zip = true;
                    startZip(f, c, locked, fightList);
                    ++fired_[zipStrike];
                } else {
                    punchUntil_ = now_ + Config::punchPulse;
                    meleeWant_ = true;
                }
                ++punches_[i];
            }
        }
        // Update 38: the web ball's aim assist - at a shot, SPIDYVR: Shooter::aim's pick (the
        // enemy nearest the hand's ray as a share of max(7 degrees, his width), within 45 m,
        // in sight); CONE 20 / WIDE 35: the nearest in that cone. The stick toward him (when
        // the player holds none), the RB a moment later so the game reads the stick first,
        // and the glob flies at him.
        for (unsigned i = 0; i < 2; ++i) {
            if (out.fired[i] != webShooter)
                continue;
            if (c.ballOurs) { // Version 0.0.2 OURS: our ball, never the game's (no RB, no stick)
                out.ballOurs[i] = true;
                out.ballDirection[i] = f.hands[i].aim;
                out.ballMetres[i] = combat38::zipFar;
                ++oursShots_;
                continue;
            }
            // Update 38.5: this press's own RB pulse, after its lead (the stick first).
            float lead = c.ballLeadAlways ? Config::assistLead : 0.f;
            bool steered = false;
            struct Queue { // queued on every way out of this iteration
                Machine& m;
                unsigned hand;
                const float& lead;
                const bool& steered;
                ~Queue() { m.queueBall(hand, lead, steered); }
            } queue{*this, i, lead, steered};
            const auto& h = f.hands[i];
            out.ballDirection[i] = h.aim;
            out.ballMetres[i] = combat38::zipFar;
            float off = 180.f;
            Vec3 centre{};
            int t = -1;
            if (c.assistSmart) {
                const Vec3 origin = h.position;
                t = combat38::smartTarget(origin, h.aim, f.enemies, count,
                                          now_ - smartAt_ < Config::lockSeconds ? smartPick_ : -1, &off);
                if (t >= 0) {
                    centre = f.enemies[static_cast<size_t>(t)] + Vec3{0, combat38::centreMass, 0};
                    smartPick_ = t;
                    smartAt_ = now_;
                    ++smartAssists_;
                }
            } else if (c.assistSpidy) {
                std::array<combat_spidy::AimTarget, combat38::maxEnemies> aims{};
                for (uint32_t e = 0; e < count; ++e)
                    aims[e] = {fightTargets[e].middle(), .45f};
                const Vec3 origin = h.position + (length(h.aim) > .5f ? normalized(h.aim) * .02f : Vec3{});
                const uint64_t unknownBefore = sightWorld.unknownAsked;
                t = combat_spidy::spidyAim(origin, h.aim, std::span<const combat_spidy::AimTarget>{aims.data(), count},
                                           sightWorld);
                sightUnknown_ += t >= 0 && sightWorld.unknownAsked > unknownBefore;
                if (t >= 0) {
                    centre = aims[static_cast<size_t>(t)].centre;
                    off = combat38::degreesOff(origin, h.aim, centre);
                }
            } else if (c.assistDegrees > 0) {
                t = combat38::assistTarget(h.position, h.aim, f.enemies, count, std::clamp(c.assistDegrees, 0.f, 45.f),
                                           &off);
                if (t >= 0)
                    centre = f.enemies[static_cast<size_t>(t)] + Vec3{0, 1.1f, 0};
            }
            if (t < 0) {
                if (count && (c.assistSmart || c.assistSpidy || c.assistDegrees > 0))
                    ++assistsMissed_;
                // Update 38.1 BALL LEAD: no target, no lead - the RB at once (ON TARGET);
                // ALWAYS: every shot's RB waits assistLead.
                continue;
            }
            out.ballDirection[i] = normalized(centre - h.position);
            out.ballMetres[i] = length(centre - h.position);
            out.assisted = true;
            out.assistDegrees = off;
            ++assists_;
            const Vec3 ahead = level(f.forward), flat = level(centre - f.head);
            if (length(ahead) > .5f && length(flat) > .5f) {
                const Vec3 right{-ahead.z, 0, ahead.x};
                assistRadians_ = std::atan2(dot(flat, right), dot(flat, ahead));
                assistUntil_ = now_ + Config::assistHold;
                lead = Config::assistLead; // the RB after the stick
                steered = true;
            }
        }
        // The aim: each shot, gadget or punch steers toward its wrist's ray.
        for (unsigned i = 0; i < 2; ++i)
            if ((out.fired[i] && out.fired[i] != sonicBurst) || out.punch[i])
                aimAt(f, c, f.hands[i].aim);
        if (out.fired[0] == sonicBurst)
            aimAt(f, c, f.hands[0].aim + f.hands[1].aim);
        if (c.steer && now_ < steerUntil_) {
            out.steering = true;
            out.stickX = std::sin(aimRadians_);
            out.stickY = std::cos(aimRadians_);
        }
        // Update 38: the web ball's assist steers on its own (aim_steer stays OFF).
        if (now_ < assistUntil_) {
            out.steering = true;
            out.stickX = std::sin(assistRadians_);
            out.stickY = std::cos(assistRadians_);
        }
        // Update 38.3 STRIKE STEER: the web strike's stick, over the ball's.
        if (now_ < strikeUntil_) {
            out.steering = true;
            out.strikeSteered = true;
            out.stickX = std::sin(strikeRadians_);
            out.stickY = std::cos(strikeRadians_);
        }
        out.aimAngle = aimRadians_ * 57.2957795f;
        out.latched = now_ - latchedAt_ < Config::lockSeconds;
        // The buttons now: the tap's RB; a gadget's RB held, its face button after a moment.
        uint16_t b{};
        // Update 38.5: the queued balls, one pulse each, in order.
        while (queued_ && now_ >= queue_[0].until) {
            for (unsigned q = 1; q < queued_; ++q)
                queue_[q - 1] = queue_[q];
            --queued_;
        }
        if (queued_ && now_ >= queue_[0].from) {
            b |= padRb;
            out.ballHand = queue_[0].hand;
            if (!queue_[0].started) {
                queue_[0].started = true;
                out.ballStart[queue_[0].hand] = true;
                ++ballPulses_;
                // the assist's stick holds while its ball's RB is down
                if (queue_[0].steer)
                    assistUntil_ = std::max(assistUntil_, queue_[0].until);
            }
        }
        if (now_ < comboUntil_) {
            if (comboRb_)
                b |= padRb;
            if (now_ >= comboUntil_ - comboSpan_ + Config::comboLead)
                b |= comboFace_;
        }
        // Update 38.2 MELEE ANIM PUNCH ONLY: the blows as spaced single X taps, the stick at rest.
        if (c.meleeEveryHit) {
            if (everyHit_.update(now_, meleeWant_)) {
                b |= padX;
                out.stickNeutral = true;
            }
        } else if (c.meleePunchOnly) {
            if (melee_.update(now_, meleeWant_)) {
                b |= padX;
                out.stickNeutral = true;
            }
        } else if (now_ < punchUntil_) {
            b |= padX;
        }
        meleeWant_ = false;
        if (now_ < zipUntil_ && now_ >= zipFrom_)
            b |= padY;
        out.buttons = b;
        return out;
    }
    uint64_t fired(uint32_t g) const { return g < gadgets ? fired_[g] : 0; }
    uint64_t punches(unsigned i) const { return i < 2 ? punches_[i] : 0; }
    uint64_t blastMissed() const { return blastMissed_; }
    uint64_t jumps() const { return jumps_; }
    // Update 38 counts (SpidyCombat38Data).
    uint64_t rejects(uint32_t why) const { return why < combat38::rejects ? rejects_[why] : 0; }
    uint64_t contactHits() const { return contactHits_; }
    uint64_t gestureHits() const { return gestureHits_; }
    uint64_t airPunches() const { return airPunches_; }
    uint64_t assists() const { return assists_; }
    // Update 38.5: ball pulses sent (one a press) and presses dropped (the queue full).
    uint64_t ballPulses() const { return ballPulses_; }
    uint64_t oursShots() const { return oursShots_; }
    uint64_t hitBalls() const { return hitBalls_; }
    // Version 0.0.2 BALL HIT +GAME WEB (experimental): the game's own ball for an OURS hit - the
    // stick toward `target` (tracking space) for assistHold, then one RB pulse after assistLead.
    void hitBall(unsigned hand, Vec3 head, Vec3 forward, Vec3 target) {
        if (hand > 1 || !finite(head) || !finite(forward) || !finite(target))
            return;
        const Vec3 ahead = level(forward), flat = level(target - head);
        bool steer = false;
        if (length(ahead) > .5f && length(flat) > .5f) {
            const Vec3 right{-ahead.z, 0, ahead.x};
            assistRadians_ = std::atan2(dot(flat, right), dot(flat, ahead));
            assistUntil_ = now_ + Config::assistHold;
            steer = true;
        }
        queueBall(hand, steer ? Config::assistLead : 0.f, steer);
        ++hitBalls_;
    }
    uint64_t ballsDropped() const { return ballsDropped_; }
    unsigned ballsQueued() const { return queued_; }
    uint64_t assistsMissed() const { return assistsMissed_; }
    uint64_t zipsAimed() const { return zipsAimed_; }
    // SpidyVR fold-in counts.
    uint64_t lunges() const { return lunges_; }
    uint64_t lungesLanded() const { return lungesLanded_; }
    uint64_t yanks() const { return yanks_; }
    uint64_t lockChanges() const { return lockChanges_; }
    uint64_t lookSwitches() const { return lookSwitches_; }   // Update 38.3
    uint64_t strikeSteers() const { return strikeSteers_; }   // Update 38.3
    uint64_t sightUnknown() const { return sightUnknown_; }
    uint64_t lockTarget() const { return lockedLast_; }
    // Update 38.2 counts.
    uint64_t targetedHeldOff() const { return targetedHeldOff_; }
    uint64_t heldOffFar() const { return heldOffFar_; } // Version 0.0.3: of those, an enemy past targetReach
    uint64_t recognised() const { return recognised_; }
    uint64_t smartAssists() const { return smartAssists_; }
    const combat38::MeleeSpacer& melee() const { return melee_; }
    const combat38::PunchTaps& everyHit() const { return everyHit_; }
    // Update 38.2 GRAB WEB: a gadget's input sent from outside (the grab web's yank: the
    // Grabber's slot, CANDIDATE), as a recognised gadget's.
    void sendGadget(Gadget g, uint32_t slot) {
        if (!slot || g >= gadgets)
            return;
        ++fired_[g];
        ++recognised_;
        startCombo(slot);
    }
    // Update 38.1: swings turned down (one each; by reason), STEADY lunges held off (his sight
    // blocked: a punch instead), lock switches the hold kept back, and the lunge in force.
    uint64_t swingRejects() const { return swingRejects_; }
    uint64_t swingRejects(uint32_t why) const { return why < combat38::rejects ? swingRejectsBy_[why] : 0; }
    uint64_t lungeHeldOff() const { return lungeHeldOff_; }
    uint64_t lockHeld() const { return lockHeld_; }
    const WebStrikeConfig& strikeConfig() const { return strikeConfig_; }
    // Update 37.2: the punch goes out as the zip strike while the nearest enemy is far.
    // Update 38: 3-20 m (DISTANCE), or the wrist's ray on a character (AIMED), or either.
    static bool zipDistance(const Frame& f, const Config& c) {
        return std::isfinite(f.enemyMetres) && f.enemyMetres >= c.zipNear && f.enemyMetres <= c.zipFar;
    }
    bool zipsNow(const Frame& f, const Config& c, unsigned hand) {
        if (!c.zipStrike)
            return false;
        const bool aimed = hand < 2 && f.hands[hand].onCharacter && !(f.enemyMetres < c.zipNear);
        const bool far = zipDistance(f, c);
        // SPIDYVR: the lunge is the zip (above); with no enemy list, AIMED's rule.
        const bool zip = c.zipMode == combat38::zipDistance ? far
                         : c.zipMode == combat38::zipAimed  ? aimed
                         : c.zipMode == combat38::zipBoth   ? far || aimed
                                                            : f.enemyCount == 0 && aimed;
        zipsAimed_ += zip && aimed && !far;
        return zip;
    }
    void reset() { *this = Machine{}; }

    // Sticky aim: the direction in force changes only when the wrist turns more than
    // lock_stick from it (or it is older than lockSeconds), so the game's pick does not
    // flicker between enemies; within aim_cone of the look the game's own pick stands.
    void aimAt(const Frame& f, const Config& c, Vec3 ray) {
        const Vec3 ahead = level(f.forward), flat = level(ray);
        if (length(ahead) < .5f || length(flat) < .5f)
            return;
        const Vec3 right{-ahead.z, 0, ahead.x};
        const float angle = std::atan2(dot(flat, right), dot(flat, ahead));
        const float cone = std::clamp(c.aimCone, 0.f, 60.f) * .0174533f * looseness(c);
        const float stick = std::clamp(c.lockStick, 0.f, 45.f) * .0174533f;
        const bool fresh = now_ - latchedAt_ < Config::lockSeconds;
        if (fresh && std::abs(angle - latchedRadians_) <= stick) {
            // held: the same enemy as before
        } else if (std::abs(angle) <= cone) {
            latchedRadians_ = 0;
        } else {
            latchedRadians_ = angle;
        }
        latchedAt_ = now_;
        aimRadians_ = latchedRadians_;
        if (latchedRadians_ != 0)
            steerUntil_ = now_ + Config::steerSeconds;
    }

  private:
    enum class State { idle, held, tapPending, spent };
    struct HandState {
        State state = State::idle;
        float since{}, punchedAt = -1e3f, age{}, ballAt = -1e3f;
        bool paired{}, targetedCounted{};
        bool seen{}, aimedLow{}, gripAtPress{}, touching{}, jumped{}, armed = true, ballFired{}, heldOffCounted{}, swingOpen{}, swingSent{}, formed{};
        float swingPeak{};
        uint32_t swingWhy{};
        Vec3 last{}, velocity{}, aimAtPress{};
        combat_spidy::Yank yank{};
    };
    // Update 38.1: a swing's candidate frames, and its one count when it ends.
    static void noteSwing(HandState& s, uint32_t why, float speed) {
        s.swingOpen = true;
        s.swingSent |= why == combat38::sent;
        if (speed >= s.swingPeak) {
            s.swingPeak = speed;
            s.swingWhy = why;
        }
    }
    void endSwing(HandState& s) {
        if (s.swingOpen && !s.swingSent && s.swingWhy < combat38::rejects) {
            ++swingRejects_;
            ++swingRejectsBy_[s.swingWhy];
        }
        s.swingOpen = s.swingSent = s.heldOffCounted = false;
        s.swingPeak = 0;
        s.swingWhy = 0;
    }
    // The zip strike's Y (zipPulse). Update 38.3 STRIKE STEER: with an enemy to strike (the
    // lunge's or the lock), the stick points at him first and the Y waits assistLead.
    void startZip(const Frame& f, const Config& c, uint64_t target, std::span<const FightTarget> enemies) {
        zipFrom_ = now_;
        zipUntil_ = now_ + Config::zipPulse;
        if (!c.strikeSteer || !target)
            return;
        for (const auto& t : enemies) {
            if (t.id != target)
                continue;
            const Vec3 ahead = level(f.forward), flat = level(t.middle() - f.head);
            if (length(ahead) < .5f || length(flat) < .5f)
                return;
            const Vec3 right{-ahead.z, 0, ahead.x};
            strikeRadians_ = std::atan2(dot(flat, right), dot(flat, ahead));
            zipFrom_ = now_ + Config::assistLead;
            zipUntil_ = zipFrom_ + Config::zipPulse;
            strikeUntil_ = zipUntil_;
            ++strikeSteers_;
            return;
        }
    }
    // Update 38.5: one press's RB pulse, after the previous one with ballGap between.
    void queueBall(unsigned hand, float lead, bool steer) {
        if (queued_ >= Config::maxQueued) {
            ++ballsDropped_;
            return;
        }
        Queued q;
        q.hand = static_cast<int>(hand);
        q.from = std::max(now_ + std::max(lead, 0.f), ballLast_ + Config::ballGap);
        q.until = q.from + Config::tapPulse;
        q.steer = steer;
        ballLast_ = q.until;
        queue_[queued_++] = q;
    }
    void startCombo(uint32_t slot) {
        comboFace_ = slotFace(slot);
        comboRb_ = slot != yAlone && slot != yHold;
        // Update 38.5 Y HOLD: Y down for yHoldSpan less the lead (0.55 s), long enough for a hold.
        comboSpan_ = slot == rbHold ? Config::yankHold : slot == yHold ? Config::yHoldSpan : Config::comboHold;
        comboUntil_ = now_ + comboSpan_;
    }
    std::array<HandState, 2> hands_{};
    float latchedAt_ = -1e3f, latchedRadians_{}, aimRadians_{}, steerUntil_{};
    float now_{}, pulseUntil_{}, comboUntil_{}, punchUntil_{}, zipUntil_{}, sonicAt_ = -1e3f,
        comboSpan_ = Config::comboHold;
    uint16_t comboFace_{};
    bool comboRb_ = true;
    uint64_t fired_[gadgets]{}, punches_[2]{}, blastMissed_{}, jumps_{};
    // Update 38
    float assistUntil_{}, assistRadians_{}, pulseFrom_{};
    // Update 38.5 ONE BALL A PRESS
    struct Queued {
        float from{}, until{};
        int hand{};
        bool started{}, steer{};
    };
    std::array<Queued, Config::maxQueued> queue_{};
    unsigned queued_{};
    float ballLast_ = -1e3f;
    uint64_t ballPulses_{}, ballsDropped_{}, oursShots_{}, hitBalls_{};
    bool farApplied_{};
    uint64_t rejects_[combat38::rejects]{}, contactHits_{}, gestureHits_{}, airPunches_{}, assists_{},
        assistsMissed_{}, zipsAimed_{};
    // SpidyVR fold-in
    Punches fists_{};
    Lock lock_{};
    WebStrike strike_{};
    std::vector<PunchEvent> events_;
    uint64_t lockedLast_{}, lunges_{}, lungesLanded_{}, yanks_{}, lockChanges_{}, sightUnknown_{};
    // Update 38.1
    WebStrikeConfig strikeConfig_{};
    float switchFor_{};
    bool switching_{};
    uint64_t swingRejects_{}, swingRejectsBy_[combat38::rejects]{}, lungeHeldOff_{}, lockHeld_{};
    // Update 38.2
    combat38::MeleeSpacer melee_{};
    combat38::PunchTaps everyHit_{};
    bool meleeWant_{};
    int smartPick_ = -1;
    float smartAt_ = -1e3f;
    uint64_t targetedHeldOff_{}, recognised_{}, smartAssists_{}, heldOffFar_{};
    // Update 38.3
    float zipFrom_{}, strikeUntil_{}, strikeRadians_{};
    uint64_t lookSwitches_{}, strikeSteers_{};
};

// Update 37.2: SLOT TEST - a hologram press that tries each gadget slot in turn: each
// press picks the next input, sends it to the game 1.5 s later (time to lower the arm and
// watch) for its hold, and logs it, so the player sees which slot fires which gadget
// (and which input a WEB YANK is). The ninth press after the last goes back to idle.
// WEB BALL TEST uses the same delayed send for one RB tap.
struct TestStep {
    const char* name;
    uint16_t buttons;
    float hold; // seconds held
};
inline constexpr TestStep testSteps[] = {
    {"RB+A", padRb | padA, .30f}, {"RB+B", padRb | padB, .30f}, {"RB+X", padRb | padX, .30f},
    {"RB+Y", padRb | padY, .30f}, {"Y TAP", padY, .12f},          {"Y HOLD", padY, .60f},
    {"RB HOLD", padRb, .60f},     {"LB+RB", padLb | padRb, .30f}, {"RB TAP", padRb, .12f},
};
inline constexpr unsigned testCount = sizeof(testSteps) / sizeof(TestStep);
struct SlotTest {
    static constexpr float delay = 1.5f;
    int step = -1;               // -1 idle; else the input picked
    float sendAt = -1, until = -1;
    bool sent{};
    uint64_t sends{};
    // The hologram press at `now`: the next input (after the last: idle).
    void press(float now) {
        step = step + 1 >= static_cast<int>(testCount) ? -1 : step + 1;
        arm(now);
    }
    // One step sent `delay` from now (WEB BALL TEST: its RB tap).
    void armStep(int s, float now) {
        step = s;
        arm(now);
    }
    // The buttons to add this frame; `justSent` the frame it starts.
    uint16_t update(float now, bool& justSent) {
        justSent = false;
        if (step < 0 || sendAt < 0 || !std::isfinite(now))
            return 0;
        if (!sent && now >= sendAt) {
            sent = justSent = true;
            ++sends;
            until = now + testSteps[step].hold;
        }
        if (sent && now < until)
            return testSteps[step].buttons;
        return 0;
    }
    const char* name() const { return step < 0 ? "OFF" : testSteps[step].name; }
    // For the hologram: 0 idle, else step + 1, | 0x100 while waiting, | 0x200 once sent.
    uint32_t pack(float now) const {
        if (step < 0)
            return 0;
        return static_cast<uint32_t>(step + 1) | (!sent && now < sendAt ? 0x100u : 0u) | (sent ? 0x200u : 0u);
    }

  private:
    void arm(float now) {
        sent = false;
        until = -1;
        sendAt = step < 0 ? -1 : now + delay;
    }
};

// Telemetry, SpidyCombatData (tools/update37_records.py).
inline constexpr uint32_t dataMagic = 0x54424d43; // CMBT
struct Data {
    // Update 37.2: version 3, 208 bytes (eight gadget counts with Web Yank and Zip Strike, jumps).
    uint32_t magic = dataMagic, version = 3, bytes = 208, on = 1;
    uint32_t tapMs = 300, gestureMs = 1200, slots{}, flags{}; // slots: grabber|sonic<<4|upshot<<8|ricochet<<12|yank<<16; flags: 2 punch, 4 palms, 8 steer, 16 zip, 32 yank
    uint64_t fired[gadgets]{}, punches[2]{}, blastMissed{};   // 64 + 16 + 8
    float peakSpeed[2]{}, yankSpeed{}, upSpeed{}, lowDegrees{}, flickSpeed{};
    float blastGap{}, blastReach{}, punchSpeed{}, aimCone{}, lockStick{}, loose{};
    float lastAim{}, pad{};
    uint64_t steers{}, gated{};
    uint64_t jumps{};          // Update 37.2: hand-speed samples dropped as tracking jumps
    uint64_t sequence{};
};
static_assert(gadgets == 8);
static_assert(sizeof(Data) == 208);
} // namespace spidy::combat_input
