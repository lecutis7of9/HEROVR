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

## 79.4 (t2188): EYE JITTER levels, JITTER CHECK (on top of 79.3 ed073f7)

The 79.3 report (ed073f7) showed EYE JITTER OFF all session (one fixed 1e-6 offset): the far-building shake was gone and
the aliasing / shimmer / slight ghosting was back, the game's TAA having no sub-pixel walk to gather. The e5d2746 walk
(about 0.88 px) shook far buildings, and the halved walk (about 0.44 px) still did. This build makes the size a level.

- Where: wrist hologram > VR SETTINGS > GRAPHICS > EYE JITTER (the last row; the row's value shows the level). Each press:
  OFF > LOW 0.10 px > MED 0.22 px > FULL 0.44 px > OFF. Live, kept in live.txt (`vr_jitter = off | low | med | full`).
  MED is the default. `vr_jitter = on` still means FULL. `vr_jitter = e5d2746` (about 0.88 px, the old full walk) is the
  hidden fifth level, live.txt only; one press from it goes to OFF. A live.txt from 79.2b / 79.3 (on / off) is set to
  med once.
- Sizes are the largest offset the eye's lens stores, in eye pixels over the Halton (2, 3) walk of 8 phases (the lens
  stores twice the jitter it is given): 2 * scale * 0.4375 px with scale 0.115 / 0.25 / 0.5 / 1.0.
- Nothing else changed: vr_aa = game, the game's TAA in both eyes, lazy models, the 79.3 TAA counter.
- JITTER CHECK: the console prints `79.4 JITTER CHECK` every 10 s (level; per eye the lens jitter given and stored, the
  share of TemporalAa runs whose job read it equal to the stored pair - expected, the lens stores 2x, -2y - to twice or once
  what was given or neither; the left-right job difference; the other TemporalAa jobs, e.g. the game's own views) and
  `79.4 JITTER LEVEL at N s` for each level change. The report JSON has `jitter_check` (the same, plus each change with its
  session second and whether it came from the hologram or live.txt; `other_jobs` lists any TemporalAa job that is not an
  eye's, which is where a distant layer would show if it has its own job; none is named as far-LOD by the game).

Test (about 5 minutes in a street): start as SPIDER-MAN; with MED look at far buildings, turn your head slowly; step the
row LOW / MED / FULL / OFF and say, per level, which shakes and which shimmers. Collect reports.


## 79.5 (includes 79.4's EYE JITTER levels)

What the 79.4 session (report game-vr-a9bc38c.json, folder `...\herovr-79-1-dual-renderer-gameplay-test\HERO-VR-79.1-DUAL-TEST-win64\HERO-VR-79.1-DUAL-TEST`)
showed and what 79.5 changes. CONFIRMED = from that report or the code; COMPILED = built in CI; not headset-verified.

1. **The 79.4 session ran the 79.3 DLL.** CONFIRMED: its `xr_hash` is ee300e76... (the 79.3 DLL), `motion_hash` 9acdee24..., `bridge_hash`
   aced20e6..., `ray_hash` 0b381603... - every module in the folder was the 79.3 build, and the hologram showed on / off for EYE JITTER (the
   79.2b toggle). The launcher's own copy logic is not at fault: it copies `build\windows-ninja\renderer-modes\<mode>\spidy_stereo_probe.dll`
   over the active DLL by SHA, and those copies were 79.3's too. The folder itself was never updated. Every test ZIP has the same artifact,
   ZIP and folder name and the reports say the same version, so nothing told the user.
   79.5: **BUILD TAG** - the DLL carries `HEROVR-BUILD-TAG 0.0.79.5 5 <commit>`; the launcher prints `BUILD: HERO VR 0.0.79.5 TEST 5 commit <sha>`
   (and says so loudly when the DLL has no tag, i.e. is older than 79.5), run_game_vr.py prints and keeps it (`build_tag` in the report), the
   console's first lines show `79.5 BUILD TAG:`, and the hologram's footer reads `HERO VR 0.0.79.5 TEST 5 <commit>`. **BUILD CHECK** -
   `BUILD-MANIFEST.txt` lists the sha256 of every module; `launch-game-vr.ps1` refuses to start the game ("BUILD CHECK FAILED") when a file
   differs (a folder that is a mix of two builds). Unzip each ZIP into a NEW EMPTY folder; do not extract over an old one.
2. **Flips are on the LEFT stick again** (79.1's 894a84e, b95b7e6's `flipStickOwns` and 844e683's landing guard are reverted): the right stick zips
   (up) and heals (down) in the air as before, no longer blocked by FLIPS. The air Y web strike from b95b7e6 stays.
3. **Web Line** (still standing, perched or on a wall only - not in the air): the second anchor is **latched for 0.3 s** (the release moves the
   hand off the surface: in the 79.4 report 14 of 26 gestures were cancelled), a **green ring / red cross marker** shows the second spot while
   the grip is held (valid / not valid), a short **haptic tick** when it turns valid, a cancel says **why** (`79.5 WEBLINE: cancelled #n - ...`),
   and the native L2 + Y timings are trimmed from 0.26 s to 0.18 s (settle .04, Y .10, tail .04).

## 79.6 (includes everything in 79.5): GAME WALL ANIMS

User (t2190): "can you just use the games own animations for wall runs and climb please". Status: COMPILED in CI only; not
headset-verified. Nothing in this build reads or forces the game's own hero state.

- **Where**: wrist hologram > VR SETTINGS > BODY AND VIEW > **GAME WALL ANIMS** (OFF / ON). It is also `live.txt game_wall_anims = off | on`
  (live; kept between sessions like EYE JITTER via `SpidyHudSettings`: changed-mask bit 23, selector 10 bit 5).
- **OFF (the default)** is 79.5's wall handling exactly: HEROVR's own wall (the body held off the wall, the game's actor in its AIR state; the game's
  wall states never run). Nothing is applied while it is OFF.
- **ON** sets `wall_crawl 1` (the game's own crawl, `Settings.walls = 0`: the swing no longer takes walls, so the game's own
  `HeroStateWallCrawl` / `HeroStateWallRun` states and animations run), `hand_climb 1` (the hand pull, the stick as plan B) and `wall_view 0` (level: the
  horizon stays world-up). Turning ON while HEROVR's own wall holds you waits (`DEFERRED` line, the row reads `ON (WAITING)`) until you are off it.
  Turning OFF puts `wall_crawl 0` and the hand climb / wall view you had before back.
- **Safety (ON only)**: a **drop** (off the game's wall with no jump / web / leap in the 0.5 s before, then falling faster than 3 m/s 0.4 s later, no web held)
  is logged, puts HEROVR's own wall back for 15 s (`RECOVER`; the row reads `ON (RECOVER)`), and a **third** drop in a session turns GAME WALL ANIMS back OFF
  (`AUTO-OFF`). A fall of 80 m or more with no web is logged `OUT OF WORLD?` (log only: there is no teleport / respawn here). A **view flip** (the view
  more than 80 degrees from level for 0.25 s while on a wall) is leveled (`GameTrackingRig::diedOrRespawned`, the death / respawn level; 2 s cooldown)
  and logged. The existing WALL TILT SAFE guard (60 degrees) still runs.
- **Diagnostics**: `include/spidy/wall_anims.hpp` Watch. Console lines `79.6 WALL <session s>: CONTACT / HAND-OVER / LEFT / DROP / OUT OF WORLD? / VIEW FLIP /
  TOGGLE / DEFERRED / RECOVER / AUTO-OFF ...` (contact, leave and hand-over lines are rate-limited to a burst of 6, then 1 per 0.8 s; everything is still
  counted) and a `79.6 WALL: GAME WALL ANIMS ON|OFF; contacts own / game / floor; hand-overs; drops; ...` summary every 20 s while something changed, and at the
  session's end `79.6 WALL (report): ...`. The report has a `wall_anims` field (counts, seconds per surface kind, the last 24 events with session seconds).
- **Not read**: the game's hero state (WallCrawl / WallRun / Perch). The surface kind comes from the swing bridge (none / HEROVR's wall / the game's crawl /
  a floor crawl), with the game's collision flags and the speed on each contact line. Whether a given moment is the game's crawl or its wall run is not
  separated, and 79.6 has not been seen on a headset: whether the game's wall run takes over at speed, the earlier run371 view inversion, and the hand-over
  from a swing are what the test must show.

## 79.7 (includes everything in 79.6)

From the 79.6 test (0e1aae5): "feet far back from the wall, waist up vanishes, the view does a 180 on the left stick, hand over hand is useless, no green dot, far buildings shake more with jitter".

1. **GAME WALL ANIMS stick / yaw.** With the level wall view (which GAME WALL ANIMS sets) the stick-to-yaw alignment that turned the tracking space 160-170 degrees as a climb began is off. The stick is laid on the wall's own up and right (forward climbs). A *yaw jump safety* replaces the tilt flip detector (which never fired): a yaw change over 60 degrees in one frame on a wall without right-stick input is put back, and `79.7 WALL ... VIEW YAW JUMP` is logged and counted (`view_flips`, `max_yaw_jump_degrees` in the report's `wall_anims`).
2. **Wall frame and stand-off (game crawl, level view).** The head stands 0.3 m off the wall (no crouch). The body solver gets the wall's normal and the head's distance to its plane; each foot that stands more than 8 cm further from the wall than the game's sole should is pressed to the plane (the game's leg pose is kept otherwise).
3. **Hand-climb reach** 0.7 m against the wall plane while GAME WALL ANIMS is ON (0.45 m otherwise).
4. **Bold Web Line marker.** While the gesture holds its grip: a filled green disc (red cross when the release would not lay a line, or the aim meets nothing) of at least 40 px, drawn whatever RETICLES says, and a preview line from the first anchor. Counters (`webline_marker` in the report): marker set / drawn per hand, first-frame target valid, valid / not valid / no-target frames, preview drawn.
5. **Slow-walk jitter.** LOW / MED / FULL walk a circle of 32 steps a turn (amplitude 0.10 / 0.22 / 0.44 px), the same phase for both eyes, so a TemporalAa job one frame behind is off by at most 0.196 of the amplitude. Per level counters (`jitter_levels` in the report): the job's lag (0 / 1 / 2 / 3+ frames) per eye and the left-right difference.

Unproven: all five need a headset. The body wall frame and the stand-off distances are guesses from the 79.6 numbers.
