# HERO VR changelog

HERO VR - Made by Agent Mulder. Full history (back to Spidy): [docs/CHANGELOG.md](docs/CHANGELOG.md).

## hero-0.0.76-sm1 - melee, web balls, flight (TEST build, not released)

From test 801ce57, then test 32bd648's feedback. **Built and unit-tested, not yet tried in the game.**

- **AMBIENT OCCLUSION ON BY DEFAULT (new live.txt `vr_ao`, on by default, next VR start).** If your Ambient
  Occlusion is Off, VR sets it to XeGTAO while it runs (soft contact shading that gives buildings and streets depth;
  about 1-2 ms a frame, CANDIDATE) and puts yours back when the game closes; `vr_ao = off` leaves yours as set. The
  comfort advice now suggests XeGTAO instead of Off.
- **ONE SETTINGS PAGE on the wrist.** VR SETTINGS and GRAPHICS are one page (it scrolls): RECALIBRATE, FPS, then
  BODY AND VIEW, WEBS, HOLOGRAM, MIRROR, SOUND, GRAPHICS and PERFORMANCE. The test and diagnostic rows (anti-aliasing,
  sharpness, history, supersample, eye AA, trail, TAA jitter, eye test, address finder, foveated debug, eye LOD,
  shared LOD, lens flares, world log, and the web ball / foveated fps / slot test presses) are gone from the
  hologram; the ones with a live.txt key still work from there. Defaults unchanged.

- **SHADOW STABLE (test; new live.txt `vr_shadow_stable`, off by default, live, no restart).** Run 3's picture
  (`vr_aa` ours, `vr_taa_depth` off, `vr_ss` off) with flickering far building shadows. Found (CONFIRMED in the
  game's code): the game's far shadow cache is one object shared by every view, remade each frame by any view the
  game is told is "active"; the left eye is told so, and so is the head's own view. The visual reports REFUTE the
  first idea (an eye getting no far shadows): every eye fit had them. CANDIDATE: the two remakes race each frame and
  the right eye takes whichever came last, so the far shadows jump. With it on the left eye no longer remakes them:
  they are made once a frame from your head's view and shared by both eyes (the same as `far_shadows` 1 for the far
  shadows; the near ones are still fitted for each eye). The game already snaps its shadows to their texels.
- **TEXTURE DETAIL (test; new live.txt `vr_texture_detail`, off by default, next VR start).** More surface detail
  without sharpening: `aniso` gives the game's material textures it left trilinear 16x anisotropic filtering (most
  were already 16x, so a small change at a small cost), `quality` sets Texture Quality Very High while VR runs (your
  setting put back after; more video memory and possibly some streaming hitches), `on` both.

- **SEAM / EDGE TRACE (diagnostics only, read-only; new live.txt `vr_taa_seam`, 0 off the default, live, no
  restart).** The visual-9 test ("doesnt settle", SpidyVR's TAA, `vr_taa_depth` off) had no depth-on logs and the
  earlier trace only ran with `vr_taa_depth` on. With `vr_taa_seam` on the trace also runs on the head-motion path:
  a separate TAA_COUNT + TAA_AGE pipeline variant reads the eyes' depth for its bins only (the history is still moved
  by the head motion, exactly as with it off). Every 10 s, by movement (still / moving / fast), angle (frontal / mid /
  grazing / silhouette) and distance (near under 8 m / mid / far over 30 m), edge vs flat: the picture before the TAA
  against what it shows (luma change in vs out, flicker share over 1/16); and for the bright frame edges (edge pixels
  whose 3x3 luma span is over 0.25): history age mean / max, the history's weight in what is shown, rejection
  clip-only / depth-only / both / neither, the 3x3 YCoCg spread and variance-box size, the 3x3 depth spread, bright vs
  dark share, and 8 of those pixels' own readings (position, age, weight, rejection, luma, spread, depth). A TAA SEAM
  VERDICT line names which of three readings matches (CANDIDATE): F1 age resets and the clip rejects (the clip too
  hard on high-spread pixels), F2 age resets and the depth rejects (the depth tolerance), F3 the age climbs and it
  still flashes (coverage: `vr_ss 110`). Nothing in the picture changes: no TAA, weight, clip, jitter, Halton or depth
  reprojection change; with it on and `vr_taa_depth` off it allocates the trace's age buffers (about 124 MB of VRAM at
  2688x2880). `vr_mip` was not done (it only changes texture sampling inside surfaces, not silhouettes).
- **Fix: send-me.zip has every recent session's logs.** collect-reports.ps1 took only the newest report's logs, so a
  test of several sessions in a row sent the last session's alone (visual-9: two earlier sessions with only their
  window-probe files). It now adds the console / game / prompts logs of every session of the last 12 hours (up to 8),
  including one still running (its console log is written as it goes); the report json stays the newest only.
- **A-depth TRACE (diagnostics only, nothing new on by default; `vr_taa_depth` stays off by default).** Every 10 s,
  for whatever `vr_aa` / `vr_taa_depth` / `vr_ss` is in force (each window is tagged with them and the eye render /
  headset size): fps and the pair-to-pair frame-time spread, render hitches (pairs 50 ms or more apart) on their own
  lines with a QPC time; with `vr_taa_depth` on: each eye's depth (this pair's and the shown one's), the depth path
  vs head-motion fallback share, the per-eye choice a per-eye gate would make (logged only: rendering stays
  both-eyes-or-neither), the convention round trip mean / max, the history age of 8 static-surface pixels per eye,
  and the counted pixels binned by the surface's angle to the view (frontal / mid / grazing / past 16x anisotropy /
  depth edge / no depth) with clip-only / depth-only / both rejection, per-pixel fallback, history age, luma change
  and depth residual, and inside each bin the EDGE pixels apart (a depth step, a no-depth neighbour or a crease: the
  corner where a wall turns into a recessed window), bright SPIKES (a pixel above its own 3x3 variance box) and
  FLIPS (luma change over 0.25 against the history), the edge luma change in vs shown (pre / post TAA) and the edge
  history age mean / max. The TAA, its weights, clip, jitter and depth reprojection are
  unchanged: the trace is a separate pipeline variant used only with `vr_taa_depth` on, which only adds counters;
  if it cannot be made the counted depth path runs exactly as before.
- **SFX DIAG (diagnostics only).** The mod's own XAudio2 sound-effects layer: a format line (mastering voice, device
  mix format and name, our voices), 10 s lines (buffers, queue, glitches, errors, audio pass gaps, late wake-ups)
  and moment lines (glitch, critical error, late pass, layer switch) on the same QPC clock as the render hitches; no
  per-callback disk logging. New live.txt `vr_sfx_layer` (0 on, the default; 1 off: only the mod's own sound effects
  stop, live, no restart).
- **vr_ss now reaches the eyes with `vr_aa = ours`.** SpidyXrStart had put every `vr_ss` back to 100% under ours and
  SMAA; with ours it now keeps 110 / 125 / 150 (per side; 110 is new, 1.21x the pixels, a test). Default (off) is
  100% as before; with SMAA it stays 100% (SMAA is skipped on supersampled eyes).

- **Fix (A-depth CHECKED v3): the TAA FRAME LOG is read and printed once per 10 s window again.** 5881bac had moved it
  under the depth check, so the launcher read its 70 KB record from the game and printed a console line with every sample
  (2701 lines in the 16:35 session instead of about 17). Launcher only; the DLL, SPIDY TAA and `vr_taa_depth` (off by
  default) are unchanged.
- **A-depth CHECKED (TEST 0.0.76 A-depth CHECKED): the depth is checked before it is trusted, and its effect is
  measured.** Still off by default (`vr_taa_depth`). (1) Convention check: about once a second (one pair in 30 until it
  passes), both eyes' centre band of depth is read back. The centre pixel is put back in the world through the inverse
  of the JITTERED eye view-projection - rebuilt from the same CONFIRMED lens inputs (0x2c75560: the eye's tangents, the
  near and far the lens was given, this pair's jitter) - and projected back through the same matrix; the error in
  pixels is logged and over 0.05 px refuses the depth. That round trip only proves the matrix and its inverse, so a
  stereo test at depth edges (each left-eye point, read in each convention, must land on the same depth in the right
  eye) tells reversed-Z / 0..1 vs -1..1 / linear vs reciprocal apart. The log line states the convention
  (reversed-Z, D3D 0..1, reciprocal, finite far); any other fitting better is FAILED with "likely convention error"
  and the depth path stays off for the session. Until a check has PASSED the history is turned by the head alone.
  (2) Rejections reported apart, never combined: clip alone, depth test alone, both, per still / moving / fast swing
  (head travelling 6 m/s or more, or turning 120 degrees a second or more), for the depth path and the head path.
  (3) Per-eye gating: if either eye's depth (this pair's or last's) is missing, both eyes are turned by the head for
  that pair; the kept pictures are the same either way, so neither history is spoilt. (4) A pixel whose depth is
  0, 1, NaN or lands outside the last eye's frustum falls back to the head-motion place (history kept, counted as
  fallback), not rejected. (5) No motion vectors, no object motion: static geometry only. (6) Every 10 s the console's
  `TAA DEPTH` line: depth found per eye %, round trip px, clip / depth / both %, fallback %, fps and the depth pass's
  own GPU ms a pair (its two depth copies plus what the TAA pass costs more with the depth). In the report's
  `taa_depth_check`. TAA core (per-eye history, Halton 16, Catmull-Rom, YCoCg clip, 0.9 / 0.6, cut reset), the lens
  call, sharpening and the game's TAA unchanged.
- **A-depth (TEST 0.0.76 A-depth): SPIDY TAA's history follows the eyes' depth.** Until now each eye's history was
  turned with the head's turn only, so windows and fine building edges that shift against the far city as your head
  moves were clipped away and started again (the aliasing and the swing smear). Now each pixel is put back in the
  world with the eye's own depth (fmt 19 at the eye's size), moved into last pair's eye and looked up there; if last
  pair's depth there disagrees, the pixel was hidden before and starts fresh. The blend (0.9 still / 0.6 moving) and
  the colour clip are 509bdc9's, unchanged. live.txt `vr_taa_depth` is `off` by default (509bdc9's history) because
  the eye depth is still CANDIDATE (test 07c1675's log: both eyes' depth in 82 of the first 200 pairs); `vr_taa_depth
  = on` tries it, `auto` turns it on by itself once the TAA WATCH depth log (200 pairs) has CONFIRMED the eye depth
  (live, no restart). A pair without both eyes' depth is turned as before, by
  itself. Moving cars and people are still kept clean by the clip only (no motion vectors). About 190 MB of VRAM
  while on.
- **TAA FRAME LOG (read-only, jitter diagnostic build):** SPIDY TAA's history pass, eye 0, the last 300 pairs: the
  view each pair was rendered with and the one its history was fetched from (matrix and lens), our sub-pixel step on
  each, the centre pixel's history UV and shift, that shift with the step's change taken out (the drift), its change
  from the pair before, and a still-head flag. Every 10 s the console prints the mean / max drift with a still head
  and the measured step gain; at the start the line worked out from the code: `matrices: jittered, jitter applied:
  once, sign: +`. The report's `taa_frame_log` keeps the stillest 300-pair block and the last one. No picture changes.
- **TAA WATCH (read-only, in every 0.0.76 A/B/C test build):** the console prints a line every 10 seconds with the
  eye fps, the GPU time a pair (the game's two eye jobs, timed at their own begin and end markers, and SPIDY TAA's own
  pass) and the still / moving split, including how many pairs with a still head were treated as moving (over 20%
  means the thresholds are wrong, not the blend). It also logs the eye depth (fmt 19 at the eye's size) for the first
  200 pairs and says CONFIRMED or CANDIDATE. All of it is in the report's `taa_watch`. Nothing to set; no picture
  changes.
- **Tests for the far windows that swim with a still head (in every 0.0.76 A/B/C test build):** each line goes in
  live.txt, takes effect at once, and can be changed between runs; without them nothing changes. `vr_taa_jitter =
  on` (the default) / `off` / `fixed` / `half` changes only our own sub-pixel step (off: none; fixed: one constant
  offset; half: the same 16 steps at half size) - our TAA and each eye's history keep running, the game's jitter is
  untouched. `vr_taa_still = 0.97` changes only how much of the old picture is kept while the head is still (this
  build's own when unset: 0.9, C-still094 0.94; the moving share stays 0.6). TAA WATCH's 10-second lines now also
  give the history rejection - the share of pixels the TAA's colour clip had to correct - with the head still and
  moving, and which vr_taa_jitter / vr_taa_still the window ran with. And a JITTER CHECK with the head still: our
  step this pair and last, how far the TAA moved its history to line it up (in pixels), and whether that counts the
  step none, once or twice (once is right; the code does it once).
- **SPIDY TAA back to 509bdc9's picture:** test 9aaedd5's tune (a longer average, a looser colour clip, a 10-bit
  history, a mild sharpen after the TAA and texture mip bias -0.5) brought the line crawl back, so all of it is taken
  back: the eyes get exactly 509bdc9's TAA again, which fixed the shake. Nothing to change in live.txt. Kept, off by
  default: `vr_ss = 125` or `150` in live.txt renders the eyes larger and filters them down first - smoother, but
  heavier on the GPU (next VR start). The report now keeps the "VR AA" record even when the game closes first.
- **Our own eye anti-aliasing (SPIDY TAA), the default:** the game's TAA no longer runs in the eyes - its one history
  was made for the flat camera, which brought the far shake and the ghosting. Each eye now has its own history in
  the mod's own pass, moved with that eye's own pose (its own view, lens and position, this pair and the one before),
  its own sub-pixel jitter, and colour clipping so moving things leave no trail. Eyes at native 100%, no upscaler,
  supersample off, so it costs little. live.txt (applies the next time VR starts): `vr_aa = smaa` gives SMAA in each
  eye instead - no history at all, so nothing can shake or ghost (a little more shimmer); `vr_aa = game` is the old
  SM1STYLE picture. If our TAA cannot start, SMAA takes over by itself. The console's "VR AA: ..." says what ran.
  Not done: the game's depth and motion vectors for the eyes are not known (no CONFIRMED addresses), so the history
  follows your head only; moving cars and people are kept clean by the clipping instead.
- **Freezes and lag fixed (HITCH HOLD):** in the last test Virtual Desktop held about one frame in ten for a tenth of a
  second, and each held frame made VR drop to the game screen for up to two seconds (the freezes, black edges and
  snaps). A held frame of up to half a second now plays as one short step and VR goes on. live.txt, live (no
  restart): `hitch_hold = off` is the old rule. If it still stutters, check Virtual Desktop's Wi-Fi, bitrate and
  codec (90 Hz). [docs/VISUAL-0.0.76.md](docs/VISUAL-0.0.76.md)
- **Double jitter fixed (JITTER FIX):** the game's own (unseen) head view and the head view the eyes' shadows are
  fitted to took the game camera's already-stored TAA jitter and gave it to the lens again, which stores twice what it
  is given and flips the up-down part, so those views jittered twice as far, upside down. They now get none. The eyes'
  own jitter is unchanged and the game's TAA stays on in each eye. live.txt, live (no restart): `vr_game_jitter =
  keep` is c71a59c's doubled jitter, to compare. The console's "JITTER LOG" lists each view's jitter for its first 200
  frames, and "POSE CHECK" now also says whether each eye's layer got the exact pose and field of view its picture was
  built from, and whether those eyes were located at the frame's predicted display time (read-only). Note: the last
  time the head view had no jitter (c8974f4), the city ghosted; if it does again, say so - `vr_game_jitter = keep`
  puts it back live.
- **City ghosting fixed (GHOST FIX):** the ghosting of the city and buildings that came with the last test is gone:
  the game's own (unseen) head view keeps its TAA jitter again, as in the sharp dd027d4 build. Taking it away (the
  last FAR SHAKE try) stopped that view's TAA and did not stop the far shake (run 005811). live.txt, live (no
  restart): `vr_game_jitter = off` is that try again, as a test; `vr_jitter = off` gives the eyes one fixed offset
  instead of their sample walk (a test). The report now says whether each eye read only its own TAA history
  ("SM1STYLE eyes: ..." at the end of the console). Far shake: still open - see
  [docs/VISUAL-0.0.76.md](docs/VISUAL-0.0.76.md); with Virtual Desktop, try Synchronous Spacewarp Disabled
  (Streaming tab) as a test.
- **Far shake: render vs. headset pose checked (POSE CHECK):** the eye-to-headset path was traced end to end: each eye
  is drawn from the same OpenXR eye pose (one xrLocateViews at the frame's predicted display time) that its layer is
  submitted with, and a picture shown twice keeps its own pose, as OpenXR wants. No mismatch was found in the code,
  so no pose was changed. Each run now measures it: "POSE CHECK render vs submit: MATCH / DIFFERS" at the end of the
  console, with each eye's difference in degrees and millimetres, how far the game world turned in your room before
  the picture was shown, and the picture's age (`pose_check` in the report). Read-only: no setting.
- **Clearer picture (VISUAL):** VR now starts the game with its Anti-Aliasing at TAA and no upscaler, so each eye
  gets the game's own TAA (SM1STYLE) at native 100% - no DLSS, FSR, FXAA or CAS on the eyes. Texture Quality goes to
  at least High and filtering to 16x while VR runs. Your own settings come back when the game closes. live.txt:
  `vr_aa = keep`, `vr_textures = keep`, `vr_render = upscaled` (each applies the next time VR starts).
  [docs/VISUAL-0.0.76.md](docs/VISUAL-0.0.76.md)
- **Suits scanned for you (AUTO-SCAN):** the launcher makes sm2scan.json by itself the first time (or after a game
  update), in the background - "Scanning your suits (first time only)" in its log. PLAY VR never waits for it.
- **WORN SUIT (CANDIDATE):** the suit probe names the Peter suit you wear ("WORN SUIT: ..." in the console) after
  each suit change, and sends it to your friend in the MP packet (he logs it; nothing draws it yet).
  See [docs/REMOTE-SUIT-0.0.76.md](docs/REMOTE-SUIT-0.0.76.md).
- **Web shooters never web-zip punch (TRIGGER GUARD):** a trigger press only shoots the web ball. The shooting arm's
  thrust no longer becomes a web strike (zip to the enemy and punch) - not while the trigger is touched, for 0.45 s
  after it, nor while the other hand shoots. Punches from a free hand, the punch dash and the close snap are as
  before. live.txt `trigger_guard = off` brings back c8974f4's rules (live, no restart).
  [docs/COMBAT-0.0.76.md](docs/COMBAT-0.0.76.md)
- **Punch dash is back (PUNCH DASH):** punch toward an enemy 2 - 15 m away and Spider-Man web-strikes (the game's Y)
  right to **that** enemy; with no enemy along the fist the old lunge still takes far swings. Enemies within 2 m get
  the punch (X) as before - never a dash at a close one.
- **Web balls are the game's own (BALL GAME):** HERO VR never draws or fires its own balls or cocoons for
  Spider-Man. Each press sends the game's web shooter (RB) on the press frame, from the wrist. Out of combat the game
  still refuses it (its own move selector) - not yet fixable; there nothing visible happens.
- **Steady picture:** every VR session turns the game's dynamic resolution off (it was already off for the tester).
  The blocky picture while swinging is most likely Virtual Desktop's stream; see "PICTURE AT SPEED" in
  [docs/COMBAT-0.0.76.md](docs/COMBAT-0.0.76.md).
- **Spider-Man punches land (MELEE SNAP):** each punch goes to the enemy you swing at (within 2 m, inside 70 degrees
  of the fist, or the nearest within 1.8 m in front) - the game's X combo plus the blow on him. With an enemy that
  close the web lunge (Y) never steals the punch. Arm swings while walking still do nothing.
  See [docs/COMBAT-0.0.76.md](docs/COMBAT-0.0.76.md).
- **Web balls every press (BALL SURE, 32bd648; BALL NOW, 8046cd4):** both replaced by BALL GAME above.
- **Flight dives and climbs where you look:** the left stick's forward follows your look up and down (no need to
  hover to go down); the right-stick climb stays.
- **Roll camera:** FLY ROLL COMFORT (the default) now keeps the view level and rolls only the body; FULL is the old
  rolled view. See [docs/FLIGHT-ROLL-0.0.76.md](docs/FLIGHT-ROLL-0.0.76.md).
- **Suits on the other player (probe 5):** still CANDIDATE, no remote-suit change yet. See
  [docs/REMOTE-SUIT-0.0.76.md](docs/REMOTE-SUIT-0.0.76.md).

## hero-0.0.76-flight - the flight rework (DRAFT branch, not released)

From hero-0.0.76-sm1 (faecbe9). **Built and unit-tested, not yet tried in the game.**

- **New flight (SM2), only for heroes who fly** (Iron Man, Superman, Thor unless his hammer flight is off, Green
  Lantern): both hands straight up, or A held, takes off. You fly on by yourself; your hands steer, smoothly.
  Left stick forward/back sets the speed (back: a hover); full forward is **SUPERSONIC**, which only makes you faster
  and never lowers or grounds you. **A** is the flight tool (held in flight: come down and land), **B** the air brake,
  **holding Y** the orientation brake (rights you), the **right stick** rolls your body round the flight axis (full
  360, holds where you leave it). The flips keep the old non-realistic sensitivity; Thor's hammer grip stays behind the
  back. While flying A is never the game's jump and B never its dodge. R3 still lands.
- **Lights through buildings (Batman report):** the game's lens flares now stay off in VR for every hero, whatever
  live.txt or the hologram's LENS FLARES row says (GAME only keeps the flat game's own setting). The report was from
  the 0.0.74 backup with the row flipped to GAME; see [docs/LIGHTS-0.0.76.md](docs/LIGHTS-0.0.76.md).
- **live.txt `fly_classic`** (off by default): on brings back the 0.0.75 flight (grip take-off, poses, trigger
  thrust); live, no restart.
- **Walls:** only Spider-Man climbs or sticks to walls (also after flight and grapples); another hero touching a wall
  is pushed off it. Wolverine's CLAW CLIMB is gone and SPIDER CLIMB ONLY off no longer lets other heroes climb.
- **SM1 add-on:** the same flight (arms up or A held, arms steer, left stick speed, SUPERSONIC, B brake, A held
  lands); iilyn's graphics untouched. Not in SM1 yet: the body roll, the Y brake and the walls rule (no CONFIRMED
  export for them). The A/B buttons need SpidyVR's bindings to name them (CANDIDATE); arms up always works.

## hero-0.0.76-sm1 - SM1 hologram (DRAFT test build, not released)

**Built and unit-tested, not yet tried in the game.**

- **SM1 wrist menu is now the SM2 hologram:** the same SM2 panel code (frame, fonts, colours, tabs, portrait tiles,
  CONTROLS / OUTFITS / POWERS, COLOR under the tiles, off by default) painted onto a quad. Point with the right hand
  and pull the trigger to pick.
- **Picking a hero now works:** PLAY AS closes the hologram (powers, flight and grapple were blocked while it was
  open, which is why the first test did nothing) and shows what the hero does. Every power shot is logged.
- Still not in: a live suit/model change (no CONFIRMED call), health rows in SM1.

## v0.0.75.2 - HERO VR 0.0.75 hotfix 2: Spider-Man 1 heroes (DRAFT, not released)

Shows as v0.0.75 (VERSION unchanged; at release: a PORT-VERSION bump and launcher build 18). From hero-0.0.76-sm1 on
v0.0.75.1 (efac6a8). **Built, not yet tried in the game.**

- **Spider-Man 1 (iilyn's Spider-Man VR mod):** Mulder's Heroes add-on now runs inside the game as an OpenXR layer,
  registered for your Windows user only while Spider-Man 1 plays. His graphics, performance and anti-aliasing are
  untouched.
- **Wrist hero menu:** opens like SM2 (right hand on the left wrist, pull the right trigger; the same click closes
  it), with the SM2 hologram look; right stick and right trigger pick. Hero select, outfits (names and artists), the holo card.
- **Flight:** Iron Man, Superman, Thor and Green Lantern hold a grip to take off and stay up; the left stick steers
  where you look.
- **Hero powers** hit the enemy his combat has ringed, with a beam.
- The Spider-Man 1 page lists which hero features Spider-Man 1 has and which not yet.
- **Hero suits in Spider-Man 1 (new, not yet seen in the game):** the Spider-Man 1 page's Install suits button fits
  the ten heroes to the game's own suit on your PC and adds them to the game's Suits menu (the way Overstrike suit
  mods work; Overstrike / ALERT by Tkachov). Pick them in the Suits menu; the wrist menu does not switch them yet.
  Remove suits puts the game's files back exactly; Game info only reads the game and saves a report. Close the game
  first; each takes effect the next time the game starts. iilyn's files are never touched.
  See [docs/SM1-SUITS-0.0.76.md](docs/SM1-SUITS-0.0.76.md).
- Everything in v0.0.75.1 (the freeze fix on load) is included.
- Credits: iilyn (Ilya Mezerowsky) for SpidyVR; the model artists; made by Agent Mulder.

## v0.0.75.1 - HERO VR 0.0.75 hotfix 1 (October 31, 2026)

**EXPERIMENTAL BUILD: 0.0.75 may crash or have issues. 0.0.63 is the most recent STABLE build** (Revert > USE 0.0.75,
USE 0.0.74 or USE 0.0.63 goes back, settings kept).

Version 0.0.75 hotfix 1 (build Update 39.8.1), launcher build 17 (0.0.75.2). It still shows as **Version 0.0.75**: the
VERSION file stays 0.0.75 (a four-part 0.0.75.1 would read as older than 0.0.75 in every existing launcher), and the
newer launcher build is what offers it to 0.0.75 players. HERO_TEST_BUILD OFF.

- **Fixed: the freeze right after loading a save in 0.0.75** (the see-through depth lock). The see-through depth
  barrier no longer holds its lock while waiting for the command list lock, so the game no longer hangs on load.
- Everything else is the same as 0.0.75.
- **Spider-Man 1:** the heroes and the wrist menu are not working in Spider-Man 1 yet; that comes in the next hotfix.
- Launcher: Revert offers 0.0.75 first, then 0.0.74, 0.0.73, 0.0.63, 0.0.62, 0.0.6 and 0.0.5.

## v0.0.75 - HERO VR 0.0.75 (October 31, 2026)

**EXPERIMENTAL BUILD: 0.0.75 may crash or have issues. 0.0.63 is the most recent STABLE build** (Revert > USE 0.0.74
or USE 0.0.63 goes back, settings kept).

Version 0.0.75 (build Update 39.8), launcher build 16 (0.0.75.1). From the hero-0.0.75 work (a823158), HERO_TEST_BUILD
OFF. Revert offers 0.0.74 first, then 0.0.73, 0.0.63, 0.0.62, 0.0.6 and 0.0.5.

- **Supersonic fixed:** flight and the shockwave follow your fist, and you stay level after the coast.
- **Only Spider-Man wall-crawls.** Other heroes no longer run up or stick to walls (Wolverine with claws out and
  CLAW CLIMB on can still climb).
- **Daredevil:** press **A** during his grapple to jump out of it.
- **Captain America** gets Daredevil's quick grapple.
- **Motorcycle for Captain America and Wolverine:** tap **X** and it arrives where you look. Grip the left
  handlebar to get on, turn the bars to steer, ride with the triggers. It is driver only (no passenger seat).
- **B** while seated recenters you in the seat.
- **Models hide behind walls** properly (better depth detection).
- **More enemy types** take damage from the heroes.
- **Thor's hammer throw** no longer lunges you forward.
- **All outfits are unlocked** from the start.
- **Launcher: Spider-Man 1 option.** Pick SPIDER-MAN 2 (HERO VR) or SPIDER-MAN 1. Spider-Man 1 runs **iilyn's
  SpidyVR** (by Ilya Mezerowsky, https://github.com/IlyaMez/SpidyVR), downloaded from his GitHub, unchanged and
  credited. The heroes add-on for Spider-Man 1 is still in progress.
- **Cleaner reports:** only a crash dump from this session is attached.
- **SM1STYLE hint:** if the game's Anti-Aliasing is not TAA, the launcher tells you to set it to TAA.

Known issues:
- Aliasing and window edges are still being improved.
- Damage to the green symbiote enemies is not confirmed yet.
- The '89 Batmobile can look grey.
- The motorcycle and the Spider-Man 1 option are untested in a headset.
- No passenger seats.

Credits: the motorcycle is "Low-Poly Sci-Fi Motorcycle - Cyberpunk Theme" by Mert Tetik (CC BY 4.0); the Spider-Man 1
option runs iilyn's SpidyVR (Ilya Mezerowsky, GPL-3.0) unchanged. Made by Agent Mulder.

## v0.0.74 - HERO VR 0.0.74 (October 31, 2026)

**EXPERIMENTAL BUILD: 0.0.74 may crash or have issues. 0.0.63 is the most recent STABLE build** (Revert > USE 0.0.73
or USE 0.0.63 goes back, settings kept).

Version 0.0.74 (build Update 39.7), launcher build 15 (0.0.74.1). From test build 10 (1d50e07) with the test notice
and TEST tag removed (HERO_TEST_BUILD OFF; the wrist shows "HERO VR 0.0.74 - MADE BY AGENT MULDER", the console
"HERO VR 0.0.74 - Made by Agent Mulder"). Multiplayer (the lobby server list) is included.

- **SM1STYLE eye anti-aliasing (on by default):** each eye uses the game's own TAA the way SpidyVR does, so edges
  shimmer much less. To go back to the old look, put `sm1style = off` in live.txt (it applies the next time VR starts).
- **Window-edge lights fixed:** lit windows no longer go dark at the outer edges of each eye.
- **The picture is back to 0.0.72's baseline**, keeping 0.0.73's flicker fixes (no lens flares or eye bloom in VR,
  steadier street lights and shadows, fewer popping far windows). 0.0.73's extra picture switches are gone.
- **Flying:** hold a grip to start flying, steering follows your body and is smoother, flips are livelier, and a tap
  on **Y** while flying looks up ahead when you lie flat. Flying no longer stalls or sinks when a hand blinks out of
  tracking.
- **Daredevil quick grapple:** a faster, smooth pull, and pressing grapple again with either baton mid-pull
  re-aims it at once (Arkham style).
- **Roster:** a **COLOR** tab in the header and a **MUSIC** tab on each hero with the music player.
- **Music kept:** your songs live in Documents\HERO VR\Music, so updates and reverts never lose them.
- **Multiplayer:** an internet **server list** (HERO VR's own free Cloudflare lobby), plus hosting and joining on
  your own network. The list only shows game names, heroes and player counts.
- **Batman:** four drivable **Batmobiles** (1989, ODAMI, FOREVER, FOREVER 2), the **Batcave** with the Batcomputer
  and a **safehouse** for the van, crime alerts with a route on the dash, glide pitch from your controllers' tilt,
  and separate belt spots for the grapple gun, batarangs and smoke pellets.
- **Cars:** the van and Batmobiles are solid on foot, don't show through walls, keep your play area facing forward
  as you get in, and turn flips off while you drive.
- **Throws** fly along your arm's swing (batarangs, shield, hammer, batons). Punisher's hook fires instantly, and no
  grapple press is lost.
- **Launcher (0.0.74.1, build 15):** safety checks (mod check, two-GPU check, clearer errors), Reports include the
  newest crash dump, and **Revert** offers **USE 0.0.73** first, then 0.0.63, 0.0.62, 0.0.6 and 0.0.5.

### Known issues (being worked on for 0.0.75)
- Aliasing is better but still needs work.
- Window edges are much better but not fully fixed yet.

## v0.0.73 - HERO VR 0.0.73 (October 29, 2026)

**EXPERIMENTAL BUILD: 0.0.73 may crash or have issues. 0.0.63 is the most recent STABLE build** (Revert > USE 0.0.63
goes back to it, settings kept).

Version 0.0.73 (build Update 39.6), launcher build 14 (0.0.73.1). From test build 6 (7af1589) with the multiplayer
test (MP SOLO TEST) left out, GAME TAA OFF by default, and the test notice and TEST tag removed (HERO_TEST_BUILD OFF;
the wrist shows "HERO VR 0.0.73 - MADE BY AGENT MULDER", the console "HERO VR 0.0.73 - Made by Agent Mulder").
The release zip carries `Models/` (with WOLVERINE's claws) and `Sounds/`.

- **GAME TAA starts OFF** (wrist GRAPHICS > QUALITY, under AA). The 0.0.73 test builds ran the game's own TAA in each
  eye (SPIDYVR) by default, and for some players that made the picture shake in motion. 0.0.73 starts with it OFF and
  leaves the game's own anti-aliasing setting alone. One press switches it back to SPIDYVR, with no restart. While
  GAME TAA is SPIDYVR (and ENGINE TAA is on in the launcher), VR sets the game's AA to TAA with no upscaler and puts
  yours back afterwards, unless the launcher's KEEP MY GAME AA is on. If you played a 0.0.73 test build, GAME TAA moves
  to OFF once. Pick SPIDYVR again and it stays.
- **The ROSTER is on the hologram**: the wrist's ROSTER opens hero tiles (MARVEL and DC tabs) with their portraits. Each
  hero has a page with PLAY AS, CONTROLS (every button explained), OUTFITS (his suits), STATS (rep, rank, level and the
  rep log) and SETTINGS (his old POWERS options). The tiles no longer flicker.
- **The PUNISHER VAN drives** (ON by default, can be switched off). Getting in no longer swings the van round. Grip the
  wheel and that hand's trigger drives (pull its stick back to reverse), while your free hand shoots. It has engine
  and door sounds and a dash screen (MAP, VEHICLE STATUS, CASES, LOCKER) that X or a poke pages through. The van is
  also much lighter on the CPU.
- **WEAPON COLLISION** (ON): thrown and held weapons (batons, shield, hammer, batarangs) hit walls, cars and the
  ground, bounce off and come home. Held weapons stop short of a surface. Weapons show in the mirror.
- **Grapple for every grappler**: the grip draws it and the trigger fires it, and it now reaches 80 m (it was 45 m).
  DAREDEVIL's grapple fires on a normal trigger press, and his silver baton tip flies on a rope to where you aim. His
  batons sit properly in your fist and flip and throw freely. The PUNISHER can grab with his hook. CAPTAIN AMERICA's
  shield stays on his arm, and he can sprint.
- **WOLVERINE**: retractable claws for every Wolverine suit, rooted in the fist and level with the hand (CLAW LEVEL).
- **IRON MAN**: the repulsor fires straight out of the open palm again (PALM BLAST), the repulsor beam is fixed, and
  HERO HANDS gives him his own hands, palm out when raised.
- Web lines and wall climbing are Spider-Man only.
- **Body**: the hologram's body calibration uses a true T-pose and fits the holsters, the holsters turn with your body,
  and flying leans with your speed (worn suits too).
- **Picture**: fewer flickering street lights (LIGHT HOLD, SHADOW MATCH) and far windows (CULL MARGIN), buildings keep
  flat play's detail in VR (WINDOW POP FIX, up to 2x), and EYE BLOOM is OFF and LENS FLARES are OFF IN VR by default
  (no strobing halos or headlight ghosts). IMAGE SPIDYVR is sharp with no smear; CLASSIC is the older look.
- **STREAMER MODE** (ON): the monitor shows the headset's view, with everything the mod draws, steadied (STREAMER
  SMOOTH) and from the eye you pick (STREAMER EYE).
- **SMOOTH TURN is on by default** (120°/s). Set it to 0 for snap turning. A 0 saved by an older launcher as its own
  default moves to 120 once.
- The launcher (0.0.73.1, build 14) opens the classic design by default again (the new design stays a switch in
  Options > Launcher), adds KEEP MY GAME AA, and credits Wolverine's claws. Before it updates, it still asks you to
  confirm that the new version is EXPERIMENTAL and that 0.0.63 is the most recent STABLE build. **Revert** offers
  **USE 0.0.63**, 0.0.62, 0.0.6 and 0.0.5.
- Not in this release: the multiplayer test (MP SOLO TEST) from the test builds.

## v0.0.72 - HERO VR 0.0.72 (October 28, 2026)

**EXPERIMENTAL BUILD: 0.0.72 may crash or have issues. 0.0.63 is the most recent STABLE build** (Revert > USE 0.0.63
goes back to it, settings kept).

Version 0.0.72 (build Update 39.5), launcher build 13 (0.0.72.1). From test build 12 (68b111d), with the test notice
and the TEST tag removed (HERO_TEST_BUILD OFF; the wrist shows "HERO VR 0.0.72 - MADE BY AGENT MULDER"). The release
zip carries `Models/` (every model and suit of test build 12) and `Sounds/`. Every hero stays OFF until picked; the
default suit is THE GAME'S OWN.

- **Two new heroes** on the wrist hologram's HERO PROFILES: **THOR** (Mjolnir sheathed on your back: reach back and
  grip to draw it, throw, recall and catch, lightning) and **GREEN LANTERN** (ring flight and constructs). One hero at a
  time, and every hero stays OFF until you pick it.
- **THOR HAMMER** (COMBAT): four real Mjolnir models, #1 BOOOOP (the default), #5 DEVILSEYE, #6 LOVE THUNDER and #3 FCCOOPER.
- **Real 3D models**: the batons, batarangs, guns, grapple gun and shield now use the artists' own geometry, UVs and
  textures, no longer flattened or one colour per triangle. A missing model shows MODEL MISSING, never a stand-in.
- **23 worn suits** (the ROSTER page on the wrist): pick a suit for each hero. A worn suit takes the game body's place
  (SUIT BODY: ARMS, FULL BODY or OFF) and shows in MIRROR VIEW. The default is still THE GAME'S OWN suit.
- **Reputation, skill trees and perks** for every hero.
- **GRAPPLE** for BATMAN, DAREDEVIL and the PUNISHER: takeoff from the ground, a sustained pull and a line drawn.
  BATMAN's trigger fires the grapple, a grip on a belt pouch draws a batarang, and his cape fans out while gliding.
- **SUPERMAN**: two thin heat-vision beams from the eyes, with sparks, a scorch mark and a looping sound.
  **PUNISHER**: drop-leg thigh holsters, the barrel along the aim, a muzzle flash and the player's own shotgun, pistol
  and reload sounds. **CAPTAIN AMERICA**: the thrown shield always flies home onto the forearm.
  **WOLVERINE**: grip-fist claws with SNIKT. **THOR**'s lightning uses the player's two lightning sounds.
- **WINDOW LIGHTS**: LOD FADE is off by default, and the game view's lens is never narrower than both eyes.
- The phone is Spider-Man only, and putting it back in the back pocket works (no stuck phone).
- **PUNISHER VAN (EXPERIMENTAL, OFF by default)**: tap X as the Punisher to summon it. It is an engine preview only:
  the van does not drive you through the city yet.
- The launcher (0.0.72.1, build 13) credits every model and suit artist. Before it updates, it still asks you to
  confirm that the new version is EXPERIMENTAL and that 0.0.63 is the most recent STABLE build. **Revert** offers
  **USE 0.0.63**, 0.0.62, 0.0.6 and 0.0.5.

## v0.0.71 - HERO VR 0.0.71 (October 27, 2026)

**EXPERIMENTAL BUILD: 0.0.71 may crash or have issues. 0.0.63 is the most recent STABLE build** (Revert > USE 0.0.63
goes back to it, settings kept).

Version 0.0.71 (build Update 39.4), launcher build 12 (0.0.71.1). 0.0.71: fixes VR not starting after Stop VR
("handle is invalid" at finding the headset); includes everything in 0.0.7. No new game address; the mod is 0.0.7's.

- Fixed: after Stop VR, PLAY VR stopped at "finding the headset" with `[WinError 6] The handle is invalid`. Stop VR
  left the launcher's standard handles closed, and the next start handed them on. The launcher now gives PowerShell
  its own input, saves and restores its handles around Stop VR and clears any dead ones; the Python side starts every
  program with its own input and repairs dead handles first.
- New Windows test (std-handles): Stop VR, then PLAY VR again, reaches "finding the headset" without the error.
- Revert still offers 0.0.63, 0.0.62, 0.0.6 and 0.0.5.
- Launcher build 12: before any update, the launcher shows that the new version is EXPERIMENTAL, may crash or have
  issues, and that 0.0.63 is the most recent STABLE build you can go back to with Revert; it updates only after Yes
  (No is the default). Older launchers (0.0.7, 0.0.63 and before) show the same warning in their own update window.

## v0.0.70 - HERO VR 0.0.7 (October 26, 2026)

Version 0.0.70 (build Update 39.4), launcher build 11 (0.0.70.1). "0.0.7" is published as 0.0.70 because versions
compare number by number (7 < 63); the trailing 0 keeps it above 0.0.63 for every launcher (docs/VERSIONING.md).
No new game address: every blow uses the CONFIRMED damage path, healing and SHIELD BLOCK the CONFIRMED HeroHealth record.

- BATMAN grapple gun and cape glide (cape_glide, bridge `SpidyCapeSample`): aim a hand at least ~10 degrees up (no enemy
  in the batarang sights) and pull the trigger to fire the grapple at a rooftop or ledge 2.5-45 m away (line drawn,
  zip at up to 24 m/s, vault over the edge); the grapple gun model ("Batman's Grapple Gun" by Falxx, CC BY 4.0,
  `Models\GrappleGun`) is drawn in the firing hand with the line from its muzzle, holstered on that hand's hip otherwise; arms out in the air glide (lean/dip one arm to turn, tuck to dive, raise
  to pull up; drop the arms or land to stop). No webs and no flight for BATMAN.
- BATMAN batarang throw: the batarang shows in a hand only while its trigger is held (never idle, never during a
  grapple press) and is thrown on release at the target the aim assist picks then, on a slight curve (~12 degrees,
  mirrored per hand) that still ends on the target; CHAIN and the return are unchanged. No target: a slight arc onto
  the aim, then straight.
- BATMAN smoke pellet: a grip press starting at the front of the belt (0.55-0.95 m below the head, within 0.09 m of
  the middle, 0.05-0.40 m ahead; clear of the hip holsters) takes a pellet; letting go throws it with the hand's
  velocity; it bursts into smoke on the floor, on an enemy or after 3 s. Smoke no longer puffs on batarang hits.
- Webs are Spider-Man's only: with any other hero the bridge gets no web triggers or grips, no aim markers, no web yank
  and no Y web strike (a far punch lunges without a line). Live, no restart.
- New heroes, all OFF by default (one hero at a time): BATMAN (batarangs with BATARANG CHAIN, grapple gun, cape glide,
  smoke pellets - the smoke is a look only), SUPERMAN (heat vision, COLD BREATH - real tick blows, nobody is frozen -, super strength, the SUPERSONIC
  dash), PUNISHER (two pistols and a sawed-off, holsters, EASY RELOAD), WOLVERINE (claws hitting 3x a punch, healing) and CAPTAIN AMERICA
  (shield worn, drawn, thrown with SHIELD CHAIN, bash; SHIELD BLOCK is an OFF-by-default experiment).
- Guns fire along the barrel: pistols and sawed-off sit in the palm from the OpenXR grip pose and shots leave the drawn
  muzzle. Daredevil's batons sit in the fist. Valve Index 15-degree tilt: unconfirmed (CANDIDATE).
- Optional models (BATON LOOK, BATARANG STYLE, GUN MODELS, SHIELD LOOK) from `Models\` in the release zip, with a
  built-in fallback. Credits: THIRD-PARTY-MODELS.md and CREDITS.md. The JOKER batarang (CC BY-NC-ND 4.0) ships unmodified
  with its licence.
- MIRROR VIEW: kept across the restart the wrist asks for, and the test turns itself OFF (saved) after its 30 s run.
- Captain America suit: a manual guide (CapSuit/, docs/CAP-SUIT-PIPELINE.md); nothing installs it.
- Launcher build 11: smoother opt-in HERO VR design (60 fps menu, seamless orb loop at the concept speed); classic stays
  the default with its fallback. Revert offers 0.0.63, 0.0.62, 0.0.6 and 0.0.5.
- Smoother cutscene starts: the mod no longer asks the game for two hidden 3D eye views (about 1-1.25 s at every
  cutscene start or resume) while the HUD panel's hook, without which they cannot be shown, is not installed (SM2:
  never yet). Cutscenes stay on the flat screen as before; GAME SCREEN 3D's text says so.
- Fixed: the launch settings' `session_off` now reaches prepare_launch on the real launch path.
- Release cleanup: test logs untracked, debug notes go to the log only, build paths removed from shipped files.

## v0.0.63 (October 26, 2026)

Version 0.0.63 (build Update 39.3), launcher build 10 (0.0.63.1). The mod is 0.0.62's plus one fix.

- Fixed: 0.0.62 failing to start VR. PLAY VR stopped at "starting the game" with `NameError: name 'session_off' is
  not defined` (run_game_vr.py read a variable of main() inside session()). The lens-flare choice now travels with
  the launch settings. New gate: tests/launch_session_tests.py runs the real launch path (main -> session ->
  prepare_display) with stand-in Windows modules, plus a pyflakes check for undefined names in tools/*.py.
- New opt-in launcher design (HERO VR orb, soundscape, big menu). Classic stays the default; the launcher asks once.
  Options > Launcher switches either way; Shift or --classic opens classic once; if the new design cannot start, the
  classic one opens and says why, once.
- Revert offers 0.0.62, 0.0.6 and 0.0.5. Updating from any earlier launcher installs 0.0.63 exactly once.

## v0.0.62 (October 26, 2026)

Version 0.0.62 (build Update 39.2), launcher build 9 (0.0.62.1). Built on 0.0.6 and its hotfix. No new game address:
every hit uses the CONFIRMED damage path, every wall test the CONFIRMED ray bridge. Graphics defaults unchanged.

- Ghost hands: EYE AA no longer keeps history far outside a pixel's neighbourhood, so fast hands leave no copies.
- BOLT WALLS: repulsor bolts, the chest beam and web balls stop at the first wall (fail safe: no answer, no wall).
- LENS FLARES (GRAPHICS, takes effect at the next VR start): OFF IN VR turns off the game's flares (headlights and
  bridge lights through walls at night); GAME, the default, leaves them.
- Combat: PUNCH HITS (GAME + INSTANT by default) lands a small hit when the fist touches; ENEMY TRACKING (LIVE by
  default) follows enemies at 30 Hz by their own positions. 0.0.6 behaviour one setting away.
- Daredevil: batons hit in the hand and on straight throws; a thrown baton bounces between up to 3 enemies (next within
  5 m, never the same twice, skipping downed enemies) and always returns. BEACONS IN VISION (ON): beacons and markers
  stay bright and coloured in the sonar.
- Iron Man: a new CHEST BEAM sound made from the player's repulsor sound, and the bolts and the chest beam shimmer like plasma.
- Iron Man: the hand blast uses a player's own repulsor sound (thank you!), fires from the palm along its normal, and
  hand repulsor blasts have a 2-second cooldown per hand (a ring on each wrist). The CHEST BEAM does 4x a blast's damage
  with its 30-second cooldown shown on the wrist ring, as before.
- REAL ANCHORS (launcher, ON): a swing starts only when the web meets a surface within 100 m; otherwise a miss and a
  buzz. OFF keeps the air swing. HIT INDICATOR (OFF) and HAND FLIPS (ON = 0.0.6).
- Launcher build 9: Revert offers 0.0.6 and 0.0.5; numeric version compare; first-run fixes for bracket folders,
  running inside the zip and antivirus removing the launcher; crash log falls back to %TEMP%.

## v0.0.6 hotfix (October 26, 2026)

Version 0.0.6 (build Update 39.1.1), launcher build 8 (0.0.6.6; build 7 = 0.0.6.5 was the first hotfix launcher).

- Launcher build 8: fixes "No working Python 3.11 or newer was found" after updating to the hotfix. The update from
  launcher build 6 kept the old mod files for the same version 0.0.6 and, doing so, left out the mod's own Python
  (python folder). The launcher now checks python\python.exe and the mod files' build after every install, update and
  start, and before PLAY VR, and downloads the ready-made mod files again by itself when anything is missing or older.
  Players never install Python. The release mod zips (0.0.5 and 0.0.6) always had their Python; a new gate checks it.
- The update from launcher build 6 also now replaces the Update 39.1 mod files with the 39.1.1 ones (the swapchain fix).
- A blank or placeholder update-repository setting falls back to lecutis7of9/HEROVR.

- VR no longer falls back to flat screen when the headset runtime refuses the eye images at start ("Create eye
  swapchain XrResult=-2", seen with Virtual Desktop). The mod tries again with the 0.0.5 settings (the same request,
  after 0.5 s and 1.5 s), then without the transfer-source usage, then another colour format, then smaller eye sizes
  (90/80/70/60%), and writes every try to the console. If all fail it keeps retrying the session every 2 s (5 s after
  2 minutes, with advice to restart Virtual Desktop and the game). No game addresses changed.

## v0.0.6 (October 25, 2026)

Version 0.0.6 (build Update 39.1), launcher build 6. Built on 0.0.5 (with its hotfixes 2 and 3).

- Hero powers with real damage: IRON MAN (repulsors, the chest beam aimed at the enemy you look at), DAREDEVIL
  (batons that chain to up to 3 more enemies, DAREDEVIL PUNCH, sonar) and HULK (charged jump, knockouts). One hero at a
  time, all OFF by default, all live.
- New Iron Man repulsor look and sound (no new render pass).
- The hologram in 8 pages with named sections; RESTART tags on the settings that need one.
- Opt-in reports (crash report, Report a bug, play stats off by default); nothing is sent by itself.
- Frame rate (Update 39.1): measured 34.5-37 new frames a second at 90 Hz, GPU-bound (the headset copy waits about
  6.6 ms for 0.19 ms of work; Spidy's own work is under about 1 ms). Session advice for 72/80 Hz and RenderScale
  90/80 (+14% / +30%, estimates from the RTX 4080 measurement); AUTO PERFORMANCE's AUTO target is half rate when full
  rate is out of reach; stalls are counted only during play. Graphics defaults unchanged. No new game address.
- VR-loss fix (Update 39.0.3): a lost OpenXR session is recovered (the mod retries every 2 s) instead of leaving the
  game flat.
- Launcher build 6: Revert > USE PREVIOUS VERSION (0.0.5) downloads and SHA-256 checks the v0.0.5 release, installs
  it and skips 0.0.6 until Updates > Update now.
- `tools\build.ps1` run by a launcher installs the release's ready-made mod files (never compiles); `-FromSource` builds.

## v0.0.5 (October 25, 2026)

- Launcher build 4 (hotfix, mod files unchanged): VR lost (e.g. back to the desktop in Virtual Desktop) or a headset
  that is not connected no longer leaves the game silently on its flat screen. PLAY VR says "Headset not connected:
  open Virtual Desktop in your headset first" and waits for it (up to 5 minutes) without starting the game; a
  Spider-Man 2 left running is offered to be closed first; when VR is lost mid-game the launcher offers to close the
  game and start VR again (VR cannot restart inside the same game process in 0.0.5).
- First public HERO VR release, with the HERO VR launcher (new background, "Made by Agent Mulder").

**Version 0.0.5 (build Update 38.9, October 25).** Built on 0.0.4. The player, after the 0.0.4 headset test: punches
still miss ("it needs to register every single hit"), the damage path should be "the same logic as a punch", a hero
height for a big skin, and a Daredevil baton that chains. Then: the beam's cooldown on the wrist, more forgiving
punches, a faster FLY with a SUPERSONIC dash, and sound effects. Every new experimental setting starts OFF; none
needs a game restart. The zips are sm2-vr-0.0.5.zip and sm2-vr-0.0.5-slim.zip (under 25 MB, no docs or images).

- PUNCH SENSITIVITY: FORGIVING (default) / MAX / 0.0.4 / STRICT (new, live). FORGIVING takes slower (0.78x of 0.0.4's
  punch speed), sloppier (0.7x the forward share, 0.75x the reach) and quicker re-punched jabs (re-armed after a
  0.4 m/s pull-back). Replay P5: rep0030 177 blows (0.0.4 151) of 169 forward fists over 1.2 m/s, rep0023 175
  (0.0.4 155) of 174, each its own X, mean delay 7-9 ms; 9/9 synthetic punches land (0.0.4 8/9: a 5-jab flurry with
  8 cm pull-backs was 1 blow, now 5); 14/14 everyday motions (waving, a hand dropped, scratching the head ...) send
  no X; run38/run381 waving with no enemy: X 0. MAX takes any forward hand (shown only, not sent, with no enemy).
- DAMAGE PATH (the same logic as a punch). Read from Spider-Man2.exe: the game's melee site 0x8558c7 builds the
  victim's handle from the actor table slot and queues a request with the DamageSystem through DirectDamage
  (RVA 0x3412ec0), which takes a pool slot from 0x3412da0 (lock xadd). STATIC-STRONG, CANDIDATE live.
  - DAMAGE PROBE (read-only, always on): the nearest enemies' hit points (BotHealth, found the way the CONFIRMED
    HeroHealth is) at 10 Hz, every fall put beside the last X press. The log says "VERSION 0.0.5 DAMAGE PROBE":
    CONFIRMED once an enemy's HP falls within 1 s of an X.
  - DAMAGE TEST: OFF (default) / ON (EXPERIMENTAL, live). ON sends one DirectDamage request per web ball or repulsor
    hit, the melee site's own way, after checking DirectDamage's bytes, the system's vtable and that the victim is a
    live actor slot; every touch is SEH-guarded and a fault turns it off (no crash). CANDIDATE until the probe sees
    an HP fall after the mod's own request with no X press (write state CONFIRMED).
  - Until then Iron Man's damage, the DAREDEVIL / HULK switches and the baton chain in the game stay unbuilt (no
    do-nothing switches); they are built as soon as the headset log says CONFIRMED.
- HERO HEIGHT: 1.00X (default) .. 1.60X (new, live). The VR body (not the view) is k times the player, so a big hero
  skin (Hulk) stands straight instead of crouching. 1.00X changes nothing.
- DAREDEVIL BATON CHAIN (logic, tests and preview; in the game once damage is CONFIRMED): a hit chains to the nearest
  other enemy within 6 m of the one hit, never one already hit on that throw, at most 3 chains (4 enemies), then
  returns; a miss or no enemy in range returns at once.
- BEAM RING: IRON MAN's chest beam cooldown is now a ring on the right wrist that fills clockwise over the 30 s,
  then flashes with a chime and a buzz when ready; the floating "BEAM n S" line is gone. Preview
  images/beam-ring-005.png.
- FLY MODE: FAST is now 40 m/s (was 28). SUPERSONIC: ON (default) / OFF and SUPERSONIC SPEED 2X (default) / 1.5X /
  2.5X / 3X of FAST (80 / 60 / 100 / 120 m/s), both new and live. Flying with both hands out (the FORWARD pose), push
  the left stick forward past 60% to dash; let go (under 40%) or drop the pose and you are back to FLY exactly as
  before. When the dash first kicks in (and again only after 1 s off): a sonic boom sound and a 0.9 s shockwave, a
  ring that rushes past you and a vapour cone flaring back. Nothing else in FLY changes: SLOW, NORMAL and the poses,
  turn, roll, rise, landing, the vignette and INVINCIBLE are as in 0.0.4. Preview images/supersonic-005.png.
  Honest: the game moves the hero at most 2 m a game step, so the top speeds need a high frame rate (at 60 fps the
  dash tops out near 105 m/s, so 3X reaches about 2.5X; at 72 fps and up all four differ). The flight speed is in the
  log ("flying n m/s now") and is CANDIDATE until a headset run; very fast flight may outrun the game's world
  streaming (low-detail buildings for a moment).
- SOUND EFFECTS: the mod's own original sounds (procedural, tools/sfx/make_sfx.py; assets/sfx): repulsor blast,
  chest beam, beam ready and sonic boom (a baton whoosh and hit are built in for the batons). They play on their own XAudio2 thread (loaded at run time;
  if audio fails: silence and a code in the log, the game and VR untouched). SFX VOLUME 100% (default) / 75% / 50% /
  25% / OFF (new, live). The game's own audio engine is not used (a hook stays CANDIDATE).
- PACKAGING: build.ps1 runs bootstrap.ps1 by itself when third_party is missing and names what is missing; the
  release gate unzips the slim zip into an empty folder, adds only what bootstrap fetches, checks every Python import
  and every file CMake names (tools/package_check.py), then builds and runs the tests there.
- Kept: no flicker, the left-wrist health ring, no kicks from waving, zip kick, grab assist, yank, WEB ROPE, graphics
  defaults, FLY MODE, INVINCIBLE, NOMINMAX project-wide and the windows header gate. Save and story flags untouched.
  Tests: 589 unit tests, tests/v005_tests.py, replay P5. "VERSION 0.0.5" on STATUS, in the log and the reports.
