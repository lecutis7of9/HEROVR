# HERO VR 0.0.76 - Spider-Man melee snap and sure web balls (SM2)

From test 801ce57 (the owner): "hits are still hard to land as spiderman and webballs only fire out like a quarter of
the time and even then only in combat".

## MELEE SNAP (include/spidy/melee076.hpp, combat_input.hpp; on by default, `Config::meleeSnap`)

Why punches whiffed in that run:
- most combat swings were held back as "too slow" because the lunge owned them (a lock on an enemy past LUNGE FROM),
  and some as "not forward" / "not extended", measured toward the lock instead of the enemy he swung at;
- the lunge's aim turned punches at a thug 1.2 - 2.7 m away into the game's Y web strike.

Now each swing picks **the enemy it is thrown at**: among the enemies in sight within 2 m (3.5 m in 32bd648) (flat on the ground, 3D in
the air), the one inside 70 degrees of the fist's way (smaller angle, then nearer, first); with none there, the
nearest one within 1.8 m in front. A swing with a snap target:
- is measured toward **him** (the usual PUNCH SENSITIVITY speed and reach, so a walking arm swing is still no punch);
- is never held back by the lunge, never the zip strike;
- sends the game's own X (its combo) and the **PUNCH HITS blow (the CONFIRMED damage path) to him**, and locks him.

Any enemy in sight within 2 m keeps the lunge (the game's Y) shut, so close fights are all punches. One long sweep
at him is one blow (no re-arm by time alone, no extra contact). Enemies behind a wall are never snapped.
Log: `VERSION 0.0.76 MELEE SNAP: <n> punches sent at the enemy swung at (<frames> frames the lunge stayed shut...)`.
Tests: tests/melee076_tests.cpp, tests/core_tests.cpp "HERO VR 0.0.76 MELEE SNAP", the replay suite (no arm-swing X).

## PUNCH DASH (test 32bd648 feedback; melee076::dash, combat_input.hpp)

The owner on 32bd648: "hits land more but now you have to run to them, before if they were far and you hit in their
direction you would kinda dash right to them with the punch". Why: in 32bd648 any enemy within 3.5 m shut the lunge,
and the 3.5 m snap turned far swings into an X in the air (or "too slow" while the lunge owned them).

Now:
- the snap (X at him) is for enemies **within 2 m** only; with one that close the lunge stays shut (never a dash then);
- **2 - 15 m along the fist** (inside 35 degrees of the punch's way, flat on the ground, 3D in the air, in sight): the
  punch itself is the game's web strike (Y) at **that** enemy - the stick is steered at him (STRIKE STEER) and he is
  locked. The smaller angle wins, then the nearer. One dash at a time (0.6 s);
- with no enemy along the fist, the lunge of 32bd648 and before (the look and the fist together) still takes far
  swings, so throws at an enemy 4 - 18 m zip as before (replay: 180 of 181 first throws).
Log: `VERSION 0.0.76 MELEE SNAP: <n> punches sent at the enemy swung at (within 2 m), <n> PUNCH DASH web strikes at one
2 - 15 m away`. Tests: tests/melee076_tests.cpp `dashTests`, tests/core_tests.cpp "HERO VR 0.0.76 MELEE SNAP" and
"Update 38 SpidyVR: ZIP MODE" (a punch thrown at him while looking away dashes). The old lock-lunge tests run with
`meleeSnap = false` (Update 38's rule).

## BALL GAME (test 8046cd4 feedback; combat_input.hpp `ballInstant`, ball_sure.hpp watch, game_xr)

The owner on 8046cd4's mod balls: "Do not revert back to ours they are awful". So for Spider-Man **HERO VR never
draws or fires its own web balls or cocoons**, in or out of combat (`ballOurs` off, and cleared again each frame).
Every press is the **game's** web ball: the RB goes to the game **on the press frame** (`ballInstant`: no wait for the
Sonic Burst's two-hand grace or the gadget forming, no lead), from the wrist (`ballLookNow` maps OURS/GLOB to WRIST
for Spider-Man). The RB queue still keeps its small gap after a pulse already in flight, so taps are not merged.

**Out of combat - not yet possible.** In 801ce57's run all 98 presses reached the game as RB, and the game fired its
ball only in combat. So the press arrives; the game's own state-based move selector refuses the web shooter when no
fight is on (the same finding the Nexus "Slingshot (Ungrounded)" mod reports about the game's transition selector).
No CONFIRMED SM2 function fires the ball or opens that gate, and inventing one is not allowed, so out of combat a
press still sends the RB and **nothing visible happens** unless the game itself fires.

BALL SURE's backup is gone; ball_sure.hpp is now a read-only watch (nothing fired, nothing written). Per press it
counts, in and out of combat, whether the wrist muzzle hook (CANDIDATE) saw the web shooter's muzzle ask within
0.3 s. Log: `VERSION 0.0.76 BALL GAME: in combat <n> presses (RB), <n> game shots seen, <n> none; out of combat ...;
<n> with no hook`. A run's out-of-combat "shots seen" is the evidence a later fix needs. Tests: tests/melee076_tests.cpp
`ballTests` (the watch), tests/core_tests.cpp "HERO VR 0.0.76 BALL GAME" (RB on the press frame in and out of
combat, never the mod's ball, 6 quick taps = 6 RB starts).

## TRIGGER GUARD (test c8974f4 feedback; combat_input.hpp `triggerGuard`, live.txt `trigger_guard`)

The owner on c8974f4: "webshooters are still finnicky, often ill hit trigger and that will trigger like a webzip to
punch". Why: the trigger held back punches only while it was down (0.25 or more). The ball fires on the press frame
and the hand is free again on release, so the shooting arm's thrust (just before the press, or its follow-through
after the release) passed the punch test, and with an enemy 2 - 15 m along the arm that was a PUNCH DASH, or the
lock lunge: the game's web strike (Y), which zips to him and punches. Run 005811: 17 web strikes = 15 PUNCH DASHES +
2 lock lunges; several within 0.2 s of a shot from the same hand or the other one.

Now (Spider-Man, `trigger_guard = on`, the default):
- a hand whose trigger is touched (0.10 or more), or was in the last 0.45 s, never sends the web strike (lock lunge,
  PUNCH DASH or zip), nor does any hand within 0.3 s of the **other** hand's trigger (two-hand volleys);
- every web strike waits 0.08 s before the Y goes; a trigger in that time calls it off (the thrust came first);
- a gesture punch (X) from the shooting hand is held back for 0.25 s after its trigger; a hand that actually
  touches an enemy still hits, and a free hand's punches, the close snap (within 2 m) and the PUNCH DASH from a free
  hand are unchanged;
- the web ball is untouched: still the game's own RB on the press frame.
`trigger_guard = off` is c8974f4's rules (live, no restart). Log: `VERSION 0.0.76 TRIGGER GUARD: <n> dashes and <n>
punches of a web-shooting hand held back, <n> web strikes called off by a trigger, <n> sent`. Tests:
tests/core_tests.cpp "HERO VR 0.0.76 TRIGGER GUARD" (with the guard off a tap and its thrust still web-strike: the
fault reproduced), tests/hero076_trigger_guard_tests.py. No game address.

## PICTURE AT SPEED ("PS2 pixel level" while swinging)

From the November 1 report (game-vr-20261101-181930) and run7:
- **Not the game's dynamic resolution.** Its log has "Dynamic resolution FPS target: 0" and its registry
  `DynamicResolutionTargetFPS = 0` (off). The eyes rendered at a steady 2688 x 2880, 100% scale; foveation and the
  eye LOD writer were off. The game's DLSS Quality applies only to the 1280 x 720 desktop window, not the eyes.
  Since 0.0.76 every VR session sets `DynamicResolutionTargetFPS = 0` anyway (tools/vr_display.py SESSION; the
  name and its encoding are CONFIRMED by that report) and puts the player's value back after.
- **Nothing of HERO VR's lowers the resolution with speed** (no adaptive scale anywhere in src/ or include/).
- **The headset stream is the likely cause.** The runtime was Virtual Desktop (a compressed video stream over
  Wi-Fi). The game made 45.7 pictures a second for a headset at 88.5 Hz, so every other headset frame is made up by
  the runtime's reprojection/spacewarp, and a fast swing changes the whole picture every frame, which a video encoder
  at a fixed bitrate turns into blocks. The log also has the stream stalling (13 frames held, 1.3 s; run7 82).
  In Virtual Desktop: a higher bitrate, HEVC 10-bit or AV1, the headset at 72 Hz (closer to the game's rate), and a
  6 GHz or wired router help; a lower render scale raises the game's frame rate.
- **Ours, at speed: no anti-aliasing.** The game's AA is DLAA, which the eyes cannot use, so the eyes are drawn
  without it; HERO VR's eye history smooths a still or slow picture but is cut when the view moves fast (8027 of
  11332 pairs cut), so while swinging the raw picture shows - sharper edges and shimmer, not a lower resolution.
  Unchanged (keeping history at speed smears, which the owner did not want).
- **Game streaming:** Texture Quality is Medium; the game streams textures as you travel, so a blurry patch that
  sharpens a moment later is the game's own streaming. Nothing in the logs proves it happened.
