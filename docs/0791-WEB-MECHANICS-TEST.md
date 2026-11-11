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

## t2161/t2165: picture and models

- ORIGINAL keeps the faster legacy picture (live.txt `vr_aa = game`: the game's TAA in each eye, SM1STYLE), as asked.
- FAR SHAKE fix (CANDIDATE, not headset-verified): the eyes' TAA jitter was twice the game's own size. The native lens
  stores twice the jitter it is given; the game's camera gives half a pixel (e5d2746 report: head view 0.31 px), the
  eyes were given a full pixel (stored -0.000651 NDC at 2688 wide = -0.875 px), so each eye's game TAA walked its
  samples up to +-1 pixel. Halved in stereo_probe.cpp eyeJitter (legacy route only; Performance / vr_aa = ours never
  uses that path). If far buildings still shake: live.txt `vr_jitter = off` (live, no restart) holds one fixed offset
  and the game's TAA keeps running (same frame rate). If that does not stop it either, say so: then it is not the jitter.
- The ZIP now carries `Models\` (123 files, the same ones v0.0.78's launcher ZIP ships under HV078\Models), and the
  DLL finds the mod folder from `HERO VR Launcher.cmd` (it only looked for `Launch Spidy VR.cmd` /
  `Spidy Launcher.exe`, so it searched beside the staged DLL in reports\stereo-modules\<sha>).

## 79.2 PROP PROBE (t2166): read-only, default OFF since 79.2b

Nothing about props changes in this build: throwing bins and boxes is still the game's own L1+R1. This build only
WATCHES the game's throwables so the next build can hand them to your hands. It writes nothing to the game, adds no
hook and sends no event. 79.2b: it is OFF by default and, when on, runs only in VR gameplay (2429458 had it on from
the moment the game loaded, which lagged the splash). Turn it on with `prop_probe = on` in live.txt once you stand in a
street (live, no restart); `prop_probe = off` stops it.

What it does (a background thread, every read SEH-guarded): it looks in the game's component registry for these
classes, each by its exact vtable AND its RTTI name read live (a mismatch turns the probe off and says so):
MeleeInteractable (the game's throwable), MeleeInteractableManager, WebYankToHeroMotion, MeleeInteractableShotMotion,
MeleeInteractableShot, MeleeInteractableYankedInjector. It logs, with the prefix `79.2 PROP PROBE`, at most 12 lines a
second and 4000 a session:
- every 10 s: manager found or not, throwables (placed), yank/shot components, the nearest throwable to you;
- `aim L` / `aim R`: the throwable nearest each hand's aim ray (within 2.5 m of it, up to 40 m);
- `... APPEARED` / `GONE`: the game's own yank / shot components coming and going (that is the game's yank);
- `... MOVED`: a throwable near you moving (speed in m/s) - the game's own throw path;
- `... words:` changed words inside a yank / shot component (the flight target, for the next build).

Test (about 3 minutes):
1. Start the game as usual and get into a street. Put `prop_probe = on` in live.txt and save; the console says
   `79.2 PROP PROBE: on`.
2. Walk to a street with trash cans, bins or boxes. Stand still 15 s (a `scan` line shows the counts).
3. Point your LEFT hand at a bin for 5 s, then your RIGHT hand at it for 5 s, then at a different prop.
4. Do ONE normal environmental throw with the game's own L1+R1 on a bin. Wait 5 s.
5. Collect reports and send them.

## 79.2b (t2168/t2169): splash lag, far shake, EYE JITTER on the wrist

- Lag from the PLAYSTATION STUDIOS splash (2429458): the prop probe started switched on inside the dll, so the
  world-log thread walked the game's whole component registry from boot, through loading, where it had been idle
  before (a0ef2c4). Now: off by default, only in VR gameplay when on, a scan only it asked for walks with a 1 ms rest
  every 1024 entries and 2 s apart, and a pointer that failed to read is not read again until the next scan.
- Far buildings shaking: t2165 halved the eyes' TAA jitter walk; it still shook. This build holds the eyes' jitter
  still by default (vr_jitter = off: one fixed tiny offset, the game's TAA still runs, same FPS route vr_aa = game).
- EYE JITTER on the hologram: VR SETTINGS > GRAPHICS > EYE JITTER (last row). ON / OFF, live, kept in live.txt
  (vr_jitter) and applied at the next start.

Test: boot, check the splash is smooth. In a street turn your head slowly looking at far buildings. Then flip
VR SETTINGS > GRAPHICS > EYE JITTER to ON and look again, then back to OFF. Say which one shakes. Collect reports.

## 79.3 (t2172): lazy models, the right eye's TAA count

- Models (65 of 67, 173 texture layers, some 240 MB on the GPU) are no longer read at start. Plain SPIDER-MAN reads
  none. The first time another hero is active, the roster opens, a hero model is asked for or PLAYER 2's props show,
  a worker reads and decodes them (the built-in looks meanwhile) and the headset's thread adopts them and uploads the
  texture array between two frames (one short hitch). Console: `MODELS (79.3 LAZY): not read at start`, then
  `reading them now for ...` and `read and decoded in X s on a worker, adopted and uploaded in Y ms`.
- Right eye TAA: the fbd8c29 report counted 2081 left / 76 right TemporalAa runs for 3695 eye jobs each. The count
  only matched a run to each eye's LATEST job (the game copies the next frame's job before the last one's TAA runs);
  unmatched runs still ran the game's TAA, uncounted. The eye is now found from the eye job table (recorded at the
  job's copy, dropped at its end); SpidySm1StyleData version 3 also counts every TemporalAa call and the console's
  SM1STYLE line gives the per-eye AA writes and queued TAA commands. Nothing the game runs changed (counting only).
- vr_aa = game and VR SETTINGS > GRAPHICS > EYE JITTER (default OFF) unchanged.

Test: start as SPIDER-MAN, play 3 minutes in a street (console: no MODELS lines but `not read at start`). Say whether
the lag / audio stutter is still there. Then switch to another hero once (models read then) and play 1 minute.
Collect reports.
