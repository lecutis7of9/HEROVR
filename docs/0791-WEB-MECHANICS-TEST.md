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
3. In the launcher pick ORIGINAL or PERFORMANCE (formerly "Revamped"),
   then start Spider-Man 2. Both modes load the SAME gameplay DLL (same SHA256); only rendering differs.

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

2) PROP GRAB / THROW  (status: NOT AVAILABLE on Spider-Man 2)
-------------------------------------------------------------
- Webbing / yanking props does NOT work in this build, and never worked in any
  SM2 build (v0.0.78 included): the SpidyVR prop catch (game_grab) has been
  switched off on SM2 since the first SM2 commit (sm2_port.hpp notPorted), and
  the Havok prop hooks (pre-collide, free body, body velocity) are not found
  for Spider-Man2.exe 2.810.0.0.
- The grip-and-yank -> game L1+R1 environment throw (79.1) only fires while the
  game shows its L1+R1 prompt, and SM2's button prompts are not readable in
  this build (game_prompts not ported: e5d2746's session report had 0 prompt
  calls), so it never fires either. The old start-up line "A web aimed at a
  throwable prop or a thug catches it ..." was wrong for SM2 and is removed.

3) ENEMY GRAB  (status: PARTIAL - native web strike + existing grab-web yank)
-----------------------------------------------------------------------------
DEFAULT (live.txt enemy_grip_split = off): combat exactly as before 79.1's
grip split (commit 9d58d65), which made combat sluggish:
- GRIP aimed at an enemy: the grab web attaches at once; pull the hand back
  toward you to send the game's own yank.
- Y in the air: straight to the game as its web strike (no delay).
OPT-IN (enemy_grip_split = on):
- ONE hand reached forward, GRIP on a targeted enemy: the game's web strike
  (native Y) on that same frame - no 0.24 s wait any more.
- Second hand GRIPS within 0.24 s on the same enemy: upgrades to the two-hand
  grab; pull both arms back for the game's yank. Physical air Y is not passed
  in this mode (9d58d65's rule).
- NOT in this build either way: moving the enemy with your hands (fling/slam/
  pin). SM2's BotStateFlung velocity write is not confirmed for 2.810.0.0.

The webline gesture never takes a grip with enemies within 30 m.

Console diagnostics: "HERO VR 79.1 WEB TEST" lines at swing start and
"79.1 WEBLINE" notes on each start / commit / cancel.

### t2161 web line fix

- ROOT CAUSE (confirmed in e5d2746's session report): the gesture compared the
  game-world aim point (SM2 metres, e.g. x 2426, z 768) with the hand's OpenXR
  tracking-space position, so the 35 m reach test failed on every grip and the
  gesture never started. It now uses the hand's world aim pose.
- Every grip press that does not start a Web Line logs ONE line (at most every
  1.5 s): "79.1 WEBLINE REFUSED #n (left|right hand): <reason> [aim=... reach=...
  air=... mode=... enemies=... nearest=...]".
- Each manual native Web Line (LEFT TRIGGER held fully, left grip released,
  both swing webs detached, then Y) logs "79.1 NATIVE WEB LINE #n: L2 held + Y
  sent to the game". Whether Spider-Man 2 forms a line there is the game's
  decision; it is not confirmed in a headset yet.

## Start-up crash bisect (t2155)

This ZIP carries two gameplay DLLs:

- the 79.1 web-mechanics TEST DLL (default), and
- the DLL built from the 0791 base commit 0e94193 (`build\windows-ninja\bisect-base-0e94193\`).

ONE switch: double-click `tools\BISECT-toggle-base-DLL.cmd` (with Spider-Man 2 closed), then press PLAY VR as usual.
The launcher console says `BISECT: gameplay DLL = 0791 BASE 0e94193` in yellow. Double-click it again to go back to
the test DLL (`Gameplay DLL: 79.1 web-mechanics test` in green). The switch is the file `USE-BASE-DLL.txt` in the
mod's top folder; deleting it does the same. The base DLL always runs the ORIGINAL renderer.

- Base DLL starts fine, test DLL crashes: the crash is in the 79.1 web-mechanics commits (2ff26ac..).
- Both crash: it is on the branch base (the 150 commits between v0.0.78 and 0e94193), or in this machine's setup.

After a failed start: Reports > Collect reports. `send-me.zip` now holds `xr-breadcrumb.txt` (the VR module's last
export entered, the VR worker's frame phase and any C++ runtime note, written so it survives a crash),
`console-tail.txt` (the last 200 session console lines) and the game's own log.

### Breadcrumb v2 (t2158)

`xr-breadcrumb.txt` now also has: the VR worker's start-up step (`init: ...` lines before `loop top`), the C runtime's
invalid-parameter note with tick and thread, up to 8 lines of `stack (tid N): spidy_stereo_probe.dll+0x...` return
addresses (resolved with the CI run's `herovr-79-1-gameplay-dll-symbols` artifact: .map / .pdb of the same build), a
`sprintf_s cut short` line when a console/log text was too long for its buffer (it used to end the game; it is now cut),
and a count of invalid parameters. After a failed start the launcher console also says whether Spider-Man2.exe is still
running and whether Steam still marks the game as running (nothing is closed automatically).

## t2161: picture and models

- ORIGINAL now uses v0.0.78's picture exactly: VR AA = ours (SpidyVR's own per-eye TAA). Since 7991b24 the launcher's
  renderer picker (tools/launcher/src/RendererModes.cs, Apply) wrote `vr_aa = game` into live.txt on every ORIGINAL
  launch: the legacy SM1STYLE route, the game's TAA in each eye with its Halton jitter, which made distant buildings
  shake. Both modes now write `vr_aa = ours`; the only difference left is PERFORMANCE's 70% marker. The next PLAY VR
  rewrites the line in your live.txt.
- The ZIP now carries `Models\` (123 files, the same ones v0.0.78's launcher ZIP ships under HV078\Models), and the
  DLL finds the mod folder from `HERO VR Launcher.cmd` (it only looked for `Launch Spidy VR.cmd` /
  `Spidy Launcher.exe`, so it searched beside the staged DLL in reports\stereo-modules\<sha>).
