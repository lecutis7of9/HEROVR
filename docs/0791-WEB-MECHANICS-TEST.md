HERO VR 0.0.79.1 - WEB MECHANICS TEST (private test build, not a release)
=========================================================================

Branch: experiment/0791-web-mechanics-test (off feature/0791-renderer-gameplay-parity).
Nothing here is headset-verified. It compiles and its unit tests pass in CI; the
headset test is yours.

START
-----
1. Unzip the whole ZIP into a new folder (not Program Files, not OneDrive).
2. Start your VR app (Virtual Desktop / Steam VR), then double-click
   "HERO VR Launcher.cmd" at the top of the folder.
3. In the launcher pick ORIGINAL or REVAMPED, then start Spider-Man 2.
   Both modes load the SAME gameplay DLL (same SHA256); only rendering differs.

1) WEB LINE GESTURE  (status: PARTIAL - drives the game's own Web Line input)
-----------------------------------------------------------------------------
- Standing, perched or on a wall (not in the air by default):
  aim one hand at a building/ledge/wall a web would stick to, SQUEEZE GRIP
  (light tick = first anchor), keep holding and sweep the hand across the gap
  to another surface at least 2 m away, LET GO while pointing at it.
- On a valid release HERO VR holds the game's L2 (aim) and taps Y (Triangle)
  - the same native Web Line input 79.1 already sends for left trigger + Y.
  Spider-Man 2 itself decides whether a Web Line can go there and where it
  goes (its own aim follows your head/camera, NOT the two hand points).
- Let go on nothing / open air / too short a span: cancelled, no input sent,
  the grip is not handed to swinging until you let go.
- Works with either hand (one gesture at a time). An enemy or prop under the
  aim is never an anchor; those grips go to the enemy / prop mechanics.
- live.txt:  webline_gesture = on (default) | off | anywhere (also in the air;
  this takes mid-air grips that aim at a wall away from swinging).
- Known: on the ground a grip aimed at a wall starts the gesture instead of a
  ground-launched swing web. Jump first, or set webline_gesture = off.
- Manual route unchanged: hold LEFT TRIGGER (aim) and press Y.

2) PROP GRAB / THROW  (status: PARTIAL - native canned throw, no physics tether)
-------------------------------------------------------------------------------
- When the game shows its L1+R1 environment-throw prompt: squeeze GRIP at the
  prop and YANK the hand back toward you (about 17 cm): HERO VR sends the
  game's own L1+R1 environment throw (its canned web-spin throw).
- NOT in this build: dragging/whipping the prop with your hand and throwing it
  by hand velocity. Spider-Man 2's prop physics hooks (Havok pre-collide,
  free-body, set body velocity) are NOT reverse-engineered for 2.810.0.0; the
  SpidyVR (SM1) adapter is disabled for SM2 (native_bodies "not ported").

3) ENEMY GRAB  (status: PARTIAL - native web strike + existing grab-web yank)
-----------------------------------------------------------------------------
- ONE hand reached forward, GRIP on a targeted enemy: after ~0.24 s the game's
  own web strike / zip (native Y).
- BOTH hands forward, GRIPS within 0.24 s on the same enemy: the existing grab
  web attaches; pull both arms back to send the game's own yank.
- NOT in this build: moving the enemy with your hand (fling/slam/pin by hand
  motion). SM2's BotStateFlung velocity write is not confirmed for 2.810.0.0;
  rope_grab only sends the game's yank.

Console diagnostics: "HERO VR 79.1 WEB TEST" lines at swing start and
"79.1 WEBLINE" notes on each start / commit / cancel.
