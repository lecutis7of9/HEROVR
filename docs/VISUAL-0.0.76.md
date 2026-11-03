# 0.0.76 VISUAL: the game's own TAA in each eye, native 100%, nothing spatial on top

Made by Agent Mulder. Branch hero-0.0.76-visual.

## What the owner saw

Distant buildings low quality until close; focus that comes and goes on distant buildings; a close brick wall that
crawls when nothing moves; a sharp/blurry pulse every few frames; ghosting on birds; never a crisp image.

## What the reports show (CONFIRMED)

- The game's Anti-Aliasing was **DLAA with DLSS** (registry AA 2 + UpscaleMethod 2 beside the game log's
  "Anti aliasing: DLAA / Upscale method: DLSS", game-vr-20261101-232025 and every other kept SM2 report).
- With an upscaler chosen the game makes **no TAA targets** (engine_taa.hpp aaMakesTargets), so SM1STYLE never ran in
  any kept report ("not used, the game's Anti-Aliasing makes no TAA targets"). The eyes got the 0.0.72 fallback:
  **our FXAA + an 80% colour history that restarts constantly (run7: 14606 restarts against 7297 blends) + CAS
  sharpen 140**, i.e. spatial AA plus sharpening: the shimmer and the sharp/blurry pulse.
- SM1STYLE still forced the game's own mip bias, which under DLSS is **-2.0 on 378 of 511 samplers**, at full-size eyes
  with no TAA to resolve it: the brick-wall crawl.
- Texture Quality was **Medium** (TextureQuality 1) in the latest session (High earlier the same day).
- SpidyVR's "eyes' distances" fix (matchDistances, view +1cc8, and the texture-request raise) never runs on SM2:
  its guard bytes are Spider-Man Remastered addresses (src/stereo_probe.cpp `distances`), so `eye_detail_frames` is 0.
  SpidyVR: without it "windows came onto buildings, people into the streets and textures into focus at about half
  the flat game's distance". The SM2 offset is **CANDIDATE** (EYE LOD, native_view::sm2DistanceScaleOffset, a live
  hologram switch, off by default); this change does not touch it.
- No FSR is forced anywhere; frame generation, dynamic resolution and the game's sharpening (Sharpness at the
  slider's 0) were already off/lowest in VR.

## What changed (live.txt, all RESTART, the player's own put back when the game closes)

| switch | default | what it does |
| --- | --- | --- |
| `vr_aa` | `ours` | 0.0.76 SPIDY TAA: our own per-eye TAA (below), the game's AA at TAA with no upscaler (AA 2, UpscaleMethod 0, CONFIRMED in 31 reports, docs/AA-0.0.73.md) only so DLAA/DLSS/FSR stay off the eyes; `keep` our TAA with the player's AA; `smaa` SMAA per eye; `game` the SM1STYLE picture (or 0.0.72 with sm1style = off) |
| `vr_textures` | `high` | Texture Quality at least High (2) and Texture Filtering 16x (4), only raised; `keep` leaves them |
| `vr_render` | `native` | SM1STYLE's eyes at 100%, the game's TAA with no upscaling; `upscaled` is SpidyVR's 67% |
| `vr_ss` | `off` | 0.0.76: `125` / `150` render the eyes that much larger per side, filtered down (Mitchell-Netravali, linear light) before SPIDY TAA; about 1.5x / 2.25x the GPU work, so off by default (509bdc9's run was GPU-bound at 45 new pictures/s) |

And: where the game's AA will not be TAA (vr_aa = keep with DLSS), SM1STYLE keeps the launcher's mip bias (+0.5)
instead of DLSS's -2.0.

Encodings (registry beside the game log's [Settings]): TextureQuality 0 Low (023723), 1 Medium (232025), 2 High
(121329, 141958), 3 Very High (grundy); TextureFiltering 0 Trilinear (023723), 3 8x (121329), 4 16x (232025).

## Still CANDIDATE / not done

- SM2's eye distance scale (EYE LOD) and the texture-request raise: not ported (no CONFIRMED SM2 bytes).
- DLAA per eye: the eyes never pass through Streamline (SL WATCH), so not possible.
- SMAA / other AA encodings, Level of Detail 4 (Very High): never seen beside a log; not offered.

## A-depth (TEST 0.0.76 A-depth): SPIDY TAA's history moved with the eyes' depth

| switch (live.txt, live, no restart) | default | what it does |
| --- | --- | --- |
| `vr_taa_depth` | `off` | `off` (0, the default while the eye depth is CANDIDATE - test 07c1675's log saw both eyes' depth in 82 of its first 200 pairs): 509bdc9's history, the head's turn only; `on` (1): the history moved with each eye's depth now; `auto` (2): on by itself once the TAA WATCH depth log has CONFIRMED the eye depth (200 pairs, 90% with both eyes' jobs, never the flat view) |

How: in depthBarrier, each eye's fmt 19 eye-sized depth (the one the TAA WATCH log follows, leaving DEPTH_WRITE in
that eye's job) is copied to a buffer (3 a eye, its own ring; the SEE-THROUGH ring untouched). The history pass's
TAA_DEPTH variant reads this pair's depth d (reversed-Z, infinite far: distance = near / d) at the pixel, rebuilds
the point (eye_history's reprojection divided by d, plus the eye's travel since last pair in last pair's view,
taa_depth::historyShift), projects it into last pair's eye and compares the depth it should have there with last
pair's own depth (2 x 2 around the place): within 10% of the larger, the history is used as before (Catmull-Rom,
YCoCg clip, 0.9 / 0.6 unchanged); none agree, the pixel was hidden last pair and starts again from this pair. Sky
(d = 0) is turned as before. A pair missing either eye's depth (or the switch off, or the pass not built) uses the
old pass, counted in TAA WATCH `depth N%` (depthPairs / depthMissed). A refused footprint or a failed buffer or
pass turns A off for the session (the console's "A-depth off for the session"), never the TAA or VR.
Not covered: things moving by themselves (cars, people) - no motion vectors; the clip still handles them.

### A-depth TRACE: per-eye depth, history age, grazing-angle and edge bins, hitches; SFX DIAG

Diagnostics only (include/spidy/taa_depth_trace.hpp, include/spidy/sfx_diag.hpp; tools/taa_depth_trace_record.py,
tools/sfx_diag_record.py). The console prints, per closed 10 s window:

- `TAA TRACE ... [vr_aa, vr_taa_depth, vr_ss, eye render -> headset size]`: fps, frame ms mean / max and spread;
  with the depth on, depth now / before per eye, depth path vs head fallback, the per-eye pick (logged only; the
  depth path stays both eyes or neither, as 5881bac: a per-eye gate would be a depth reprojection change), split %,
  round trip mean / max (limit 0.05 px).
- `TAA TRACE AGE`: 8 static-surface pixels per eye (3 m to 300 m): `match` pairs in a row through the depth, `kept`
  pairs in a row the clip also left alone, per still / moving / fast.
- `TAA TRACE GRAZE`: the counted pixels (1 in 16 x 16) by |n.v| (frontal >= .5, mid .25-.5, grazing 1/16-.25,
  beyond_aniso < 1/16, depth_edge, no_depth): clip-only / depth-only / both / neither, fallback, age, luma change,
  depth residual (the reprojection's own error; the round trip is a centre-pixel check, so it has no per-bin value).
- `TAA TRACE EDGES`: inside each bin, the edge pixels (10 % depth step, a no-depth neighbour, or a crease: the depth
  slope changes by over 25 % across the pixel - a recessed window's corner) with their own rejection split and flip
  rate against the pixels off edges; SPIKES (a pixel's luma above its own 3 x 3 mean + gamma sigma: one bright
  specular / sub-pixel sample the variance box cannot hold) and how often the clip moved the history there. Also,
  for the edge pixels: the luma change going IN (this pair's input vs the history read) and SHOWN (the TAA output vs
  the same history read) with their ratio (D3: pre- vs post-TAA), and the match age mean / max at edges (D2: age
  climbing = the history is kept across the corner; staying near 0 = discarded).
- `RENDER HITCH` / `SFX MOMENT` lines with their QPC time and box-local wall time.
- `SFX DIAG format` once, `SFX DIAG` every 10 s.

Reading the recessed-window sparkle (D1: a depth seam is there when the grazing bins' edge share is well above 0
and the crease test fires; no edge share at a flickering frame = normal-map / parallax detail, not geometry): shown /
in near the history weight's 0.1-0.4 = the TAA damps an input flicker (source is pre-TAA: geometry or shading);
near 1 = the history is discarded there (rejection passes it through); above 1 = the clip itself moves the
history further than the input did (rejection amplifying).
 high flip % on edges in the grazing / beyond_aniso bins with clip-only
dominating and depth-only near 0 = geometric / specular sub-pixel aliasing the variance box cannot settle (not the
depth); a large depth-only share on edges = the disocclusion test refusing history at the corner; high spike % with
a high clip-moved share = bright single samples (specular) flipping the history.

### SEAM / EDGE TRACE (`vr_taa_seam`): why the frame edges do not settle

The user's visual-9 test (SpidyVR's TAA, `vr_taa_depth` off, `vr_ss` 100, a still head looking down an avenue):
"doesnt settle". The close-up shows thick extruded window-frame slabs standing proud of a deep dark recess, so the
flash is at depth steps and creases with a large bright / dark contrast (CONFIRMED from the frames). The prime
suspect is the history being rejected at those pixels (the variance clip clamping hard on a large 3x3 YCoCg spread,
or a depth disocclusion), CANDIDATE until the trace below reads it.

`vr_taa_seam` (live.txt, 0 off the default, live, no restart) is read-only: with `vr_taa_depth` off it uses a
TAA_COUNT + TAA_AGE variant that reads the eyes' depth for the bins only (the history moves with the head motion as
without it); with `vr_taa_depth` on the A-depth trace variant also fills the seam cells. Lines every 10 s:

- `TAA SEAM <still|moving|fast>`: per angle (frontal / mid / grazing / silhouette) x distance (near < 8 m, mid, far >
  30 m) x edge / flat: pixels, the luma change before the TAA (this frame against the history's place) and after
  (what is shown against last frame's shown), and the flicker share over 1/16 of each.
- `TAA HOT EDGES <category>`: the bright frame edges (edge pixels with a 3x3 luma span over 0.25), per angle: history
  age mean / max, the history's weight in what is shown, rejection clip-only / depth-only / both / neither, 3x3
  YCoCg spread, variance box, 3x3 depth spread (relative), bright share, and in / out luma change.
- `TAA HOT EDGE SAMPLES`: 8 of those pixels from the fastest category's pass (the first 8 the GPU claimed, so
  CANDIDATE-representative; the aggregate counts every one of them on the 1-in-256 grid).
- `TAA SEAM VERDICT`: the fastest category with 20 or more hot readings, grazing / silhouette first. **settles**
  (shown / in at most 0.3 and flicker under 5%); **F1** (age under 4 and the clip rejects most): next would be a
  wider or replaced clip for high-spread pixels only, watching for smear; **F2** (age under 4 and the depth rejects
  most): a depth tolerance scaled by the depth gradient, not combined with F1; **F3** (the age climbs and it still
  flashes): coverage, `vr_ss 110` (110 per side, 1.21x the pixels; RESTART) with fps, GPU ms and repeated frames
  read. Neither F1 nor F2 is built; each would be one variable in its own test.

`vr_mip` (texture LOD bias) was not done: it only changes texture sampling inside surfaces and would not fix
silhouettes.

### A-depth CHECKED: the depth's convention checked first, rejections split, per-eye gating

- Convention check (taa_depth_check.hpp, native_eye_gpu noteDepthCheck): one pair in 30 until it passes, then about
  one a second, a 33-row band about each eye's centre row is copied to a readback on the history's list. The centre
  pixel (or the nearest on that row with a usable depth) is unprojected through the inverse of the jittered eye
  view-projection rebuilt from the lens's CONFIRMED inputs (0x2c75560: tangents, near, far - noted beside the
  unchanged lens call - and this pair's jitter) and projected back through the same matrix: the error in px is
  logged, over 0.05 px refuses. Because a same-matrix round trip is ~0 for any convention, the stereo test decides
  the convention: at depth edges in the band, each left-eye point read in each convention is projected into the right
  eye and must meet that eye's depth (3%). The log states "lens convention D3D 0..1, reversed-Z, reciprocal, finite
  far"; another convention fitting best is FAILED ("likely convention error: the depth is ...") and the depth path
  stays off for the session. Until PASSED: head motion only (`unchecked` in the line).
- Rejections apart: one pixel in 16 x 16 of both eyes is counted by the shader as clip alone, depth test alone or
  both (the clip is still evaluated on a depth-refused pixel, only to count it), per path (depth / head) and movement
  (still as TAA WATCH; fast swing: 6 m/s travel or 120 degrees a second turn; moving: the rest).
- Per-eye gating: both eyes' depth this pair and last pair, or both eyes use head motion that pair (`gated`).
- Invalid depth (0, 1, NaN, outside last pair's eye): that pixel uses the head-motion place, history kept
  (`fallback`), never rejected.
- The depth is read in the checked convention (finite far: n/z = d (1 - n/f) + n/f), each pair with its own near and far.
- Console every 10 s: `TAA DEPTH ...s: fps, depth found L/R %, used % (gated, unchecked, refused), round trip px,
  depth pass ms GPU a pair (copies + draw with depth - draw without) | DEPTH PATH still/moving/fast clip/depth/both/
  fallback | HEAD PATH ...`. Success: fast-swing rejection (clip + depth + both) on the depth path below the head
  path's 87-88%.

## FAR SHAKE (the far background hopping while the head turns)

| switch (live.txt, live, no restart) | default | what it does |
| --- | --- | --- |
| `vr_game_jitter` | `off` | 0.0.76 JITTER FIX: the game's own head views (never shown) give the lens 0; `keep` is c71a59c's: the stock camera's stored jitter passed back in, which the lens doubled and flipped (see JITTER FIX below). Zero means that view's own TemporalAa skips (as c8974f4, which coincided with ghosting) |
| `vr_jitter` | `on` | the eyes' Halton (2, 3) sample walk (the sharp bricks); `off` holds one fixed tiny offset (1e-6 NDC) so the game's TAA still runs - a test, edges a little rougher |

What run 001842 ruled out: the eyes' previous cameras are their own (9040/9040, 0.0 m); both eyes' jitter is the
same point in their own NDC, under half a pixel; no depth goes to the runtime (Virtual Desktop's reprojection is
rotation-only); EYE LOD is off and the video shows no popping. Left then: the game view's jitter and,
outside the game, Virtual Desktop's Synchronous Spacewarp at 47 pictures for a 90 Hz headset (try it Disabled).

### GHOST FIX (run 005811, c8974f4)

The owner's run of c8974f4 (vr_game_jitter off by default): the far shake unchanged, and new "ghosting of the city and
buildings". engine_taa game_jitter -0.0 (the zero reached the game), so the game view's TemporalAa skipped every frame
(0x2c2dc60 skips a zero jitter, CONFIRMED). That is the one picture change from dd027d4 (no ghosting), so `keep` is the
default again. How the skipped view reaches the eyes is CANDIDATE (frame-wide work taken from the main view's TAA
output); the frame rate also fell, 47.6 to 44.2.

Checked against the outside advice:
- Each eye its own TAA history (CONFIRMED by disassembly, 2.810.0.0): an eye job's current and history buffer sets
  are job +0x18e0 / +0x18e8, set by the job start (0x2c98440) to the eye's own set (eye +0x1e38); only a job given the
  game's main set (+0x13f9) ping-pongs [0xc15d7d8] / [0xc15d7e0] (0x2c96f80). TemporalAa writes its output into that
  set's history target (0x2c2e783). Nothing found wrong; measured each run now (SpidySm1StyleData version 2, report
  `sm1style_eyes`, console "SM1STYLE eyes: ... shared N").
- Each eye's own previous view-projection for motion vectors and reprojection: eye_camera previous_own 19916/19916.
  Nothing found wrong.
- No FXAA/SMAA added (the eyes' TAA stays; FXAA was the old soft look).

Far shake: still open. Run 005811: 44.2 pictures a second on an 89.3 Hz headset (each shown ~2 refreshes, 87
submits a second, 39 of them a picture sent again), GPU-bound. Below the refresh, Virtual Desktop's Synchronous
Spacewarp (if Automatic/Always) synthesises frames, which warps and ghosts far buildings; set it Disabled in VD's
Streaming tab, or lower the refresh rate / render size so the game holds full rate, as a test.
vr_jitter off does not move the mip level (the game's TAA keeps its own bias), so no mip-bias change goes with it.

### POSE CHECK (render vs. submitted pose)

Checked in the code: each eye's picture and its headset layer come from one xrLocateViews call at the frame's
predicted display time; the picture's own tracking eye (kept with its serial) is what the layer is given, never a
newer one; both eyes of a pair carry the same serial and generation; a picture shown on two refreshes keeps its pose
with the new display time (the runtime reprojects it, as OpenXR wants). No stale pose, wrong display time or
late head update was found, so nothing was changed. Not checked before: the eyes' rotation (the old audit compared
positions only).

Each run now measures it (report `pose_check`, console "POSE CHECK render vs submit"):

| field | meaning |
| --- | --- |
| `left` / `right` `max_degrees`, `mean_degrees`, `max_mm`, `mean_mm` | the camera the game really drew the eye from against the pose its layer was given (travel taken off); `over_degrees` / `over_mm` count submits past 0.05 deg / 2 mm |
| `command_max_degrees` / `command_max_mm` | the drawn camera against the eye HERO VR asked for: a difference in both is the game moving its camera itself |
| `world_turn` | how far the picture's world turned in the room between its command and its submit (the player's turns, flips and tilt; reprojection does not undo it) |
| `image_age_ms` | display time of the submit minus the picture's own predicted display time (about 11 ms on a reused picture at 89 Hz) |

MATCH with a small `world_turn`: the far shake is not a pose mismatch. DIFFERS: the numbers say which eye and
whether the game (command) or the hand-off (submit) moved it.

### HITCH HOLD (run 060032, c71a59c: "completely unplayable from lag")

The lag was not the game drawing slower. Virtual Desktop held the frame hand-off (xrEndFrame) for about a tenth of a
second 108 times (10.8 s of a 109 s run; holds of 100, 200 and up to 410 ms), starting on the game screen 7 s in,
before any VR eye was drawn; the two runs before (dd027d4, c8974f4) had none with the same settings. Each held frame
was more than 0.1 s long, which HERO VR took as "tracking lost": VR play dropped to the game screen for 0.2-2 s and
the webs let go, so one short hitch became a freeze of up to two seconds with black edges and a snap back. Now a
held frame of up to half a second plays as one 0.1 s step and VR goes on (live.txt `hitch_hold = on`, the default;
`off` is c71a59c's rule, live, no restart). The holds themselves come from the stream to the headset (CANDIDATE:
Wi-Fi, bitrate or codec in Virtual Desktop; keep it at 90 Hz).

### JITTER FIX (owner: "double jitter")

CONFIRMED in the code: the lens 0x2c75560 stores +0x428 = 2 * its jitter x and +0x42c = -2 * its jitter y (constant
-2.0 at RVA 0x53197c8, docs/AA-0.0.73.md). The game's head view (stereo_probe activeView, both lens calls) and the
aligned head view (placeEyes onHead) passed the stock camera's stored +0x428/+0x42c back in, so they stored 4x the
game's x jitter and +4x its y jitter (2x the game's stored values, y flipped). They now pass 0 (`vr_game_jitter = off`,
the default; `keep` is c71a59c's, live). The headset eyes' lens keeps eyeJitter's raw values. Not changed: the game
screen in 3D (desired.enabled 3) without eye TAA still passes main's stored pair to the eyes (not the headset path).

Each run logs it (report `jitter_log`, console "JITTER LOG"): per view (head_view, aligned_head, eye_left, eye_right;
`_stereo_screen` for the 3D game screen) and per lens call in a frame (`pass`), the source camera's stored pair, the
pair passed and the pair the new view stored, for each view's first 200 frames (SpidyJitterLogData, at most 2048
entries; read-only). `fed the source's stored pair back Nx` is c71a59c's doubling; 0 with `off`.

POSE CHECK (read-only) now also compares each eye's layer (the pose and fov given to xrEndFrame) with the eye pose and
lens its view and projection were built from (the command's `eyes` and `fovs`), and counts submits whose eyes were
located (xrLocateViews) at a time other than the frame's predicted display time (`layer`, `located_off`; console
"Layer vs build: SAME / MISMATCH"; the first 10 mismatches go to the prompts log). Its cost per submit is a few
matrix compares and two interlocked increments, microseconds: not the freezes (those started on the game screen,
before any eye or POSE CHECK ran - see HITCH HOLD).

## SPIDY TAA (0.0.76): our own per-eye TAA, the default

Why: the game's TemporalAa (command 0xb1, handler 0x2c2dc60, CONFIRMED, include/spidy/engine_taa.hpp) keeps one
history and reprojects with the flat camera's matrices; run once per eye it ghosts (wrong matrices) and shakes far
away (the two eyes' samples ping-pong). It now cannot run on the eyes: it needs job +0x570 == 1, a non-zero jitter at
+0x428/+0x42c and TAA targets, and the eyes get TAA targets only with eyeTemporal 0x10 (ENGINE TAA) or 0x20
(SM1STYLE) and jitter only under those - both locked off (stereo_probe SpidyEngineTaa refuses, auto never arms).

What runs (src/native_eye_gpu.cpp `taaShader`, PSO `history.taa`, include/spidy/vr_aa.hpp):
- one history picture per eye; each reprojected by a homography from that eye's own shown and current view (its
  pose and its jittered lens tangents from the submitted XrView), never the flat camera;
- per-eye Halton (2,3) lens jitter (eye_taa.hpp, the same lens the layer is submitted with);
- 3x3 YCoCg neighbourhood: variance box (mean +- gamma sigma, gamma 1.25 still -> 0.85 moving) intersected with the
  min/max box, the history clipped toward the box centre; Catmull-Rom history sample; 1/(1+luma) weighting;
- history share 0.9 still easing to 0.6 while turning/moving, 0 on a camera cut (> 0.26 rad or 3 m in a pair).
Not feasible: depth / motion-vector reprojection - no CONFIRMED eye depth or motion-vector addresses, so the history
follows the head only; moving objects are handled by the clip. Cost: one full-screen pass per eye (as EYE AA's).

`vr_aa = smaa`: SMAA 1x (preset HIGH, luma edges; iryoku/smaa, MIT) per eye, 3 passes, no history. Also the
automatic fallback when SPIDY TAA's pass cannot be built; FXAA if SMAA cannot either; the plain copy after that.
The report's `vr_aa` and the console's "VR AA: ..." line say what ran (SpidyVrAaData).

### SPIDY TAA TUNE (0.0.76): tried in 9aaedd5, taken back

9aaedd5 tried, over 509bdc9: history share 0.94 still / 0.8 moving (easing from 0.012 rad / 0.03 m), clip gamma 1.5 /
1.0, an R10G10B10A2 history, RCAS 0.15 after the TAA, mip bias -0.5 (88 of the game's negatively biased samplers).
The player: "a step in the wrong direction, worse fps and brought the line crawl back". All of it is taken back; the
eyes get 509bdc9's TAA, blend and mip bias (the game's) again.

The two runs (Quest 3 / Virtual Desktop, 2688x2880 per eye, same game settings, live.txt without vr_aa or vr_ss):

| | 509bdc9 (11:26 EST) | 9aaedd5 (12:04 EST) |
| --- | --- | --- |
| new pairs/s (play_rate), slowest tenth | 45.0, 38.5 | 46.2, 40.4 |
| session fps high / average | 67 / 45.0 | 56 / 44.5 |
| headset copy's GPU wait | 5.7 ms | 5.3 ms |
| TAA pass, GPU a pair | 0.46 ms | 0.55 ms (+0.09: the RCAS draw over the copy) |
| pairs eased for motion, least kept | 2567 of 6995 (37%), 0.6 | 2157 of 4207 (51%), 0.8 |
| new-picture gaps >= 0.1 s in play, per minute | 13.6 | 14.7 |
| runtime holds while playing | 0 (99 at the close) | 0 (75 at the close) |
| mip bias | game's (-1.0), 0 changed | -0.5 on 88 samplers |

So the frame rate did not measurably drop (GPU-bound alike); the regression is the picture. The likeliest causes, in
order: the RCAS sharpen (it raises the frame-to-frame difference of thin, jittered lines, which is what crawl is), and
keeping 0.8 of a depth-less (head-only) reprojected history while moving (half the pairs) with the looser clip, which
holds a misplaced line longer then snaps it. The mip bias (less negative is softer, never more aliased) and the
10-bit history (finer only) are unlikely causes, but nothing in the report separates them, so they go too.
Kept: live.txt `vr_ss` (off unless set, start-only) and the report's last vr_aa / jitter_log records.
Next, one at a time, each as its own test: mip bias -0.5 alone; then the history share 0.94 still only (moving
unchanged); never a sharpen in the history loop. SMAA before the TAA (T2x) only with headroom.

### TAA WATCH (0.0.76 A/B/C tests): fps, GPU ms a pair, still / moving split, the eye depth log

Read-only, in every A/B/C test build (include/spidy/taa_watch.hpp, `SpidyTaaWatchData`, tools/taa_watch_record.py).
Every 10 s the console prints `TAA WATCH <s>s: <fps> fps, eye <ms> ms, taa <ms> ms, still <%> eased <%> moving <%>,
still head <n> (<%> classed moving)`, and the report's `taa_watch` keeps up to 90 windows plus the session's totals.

- **fps:** the eye pairs through SPIDY TAA's history pass a second.
- **GPU ms a pair, eye:** the game's two eye jobs from the first begin to the last end, from a timestamp of ours beside
  the game's own at each job's begin and end marker (the jobs themselves are not changed). For B-ss125 this is the
  number for the 2 ms rule: compare it with a vr_ss 100 run.
- **GPU ms a pair, taa:** our own TAA pass, both eyes.
- **Still / moving split:** still = kept the full still share; eased = kept less (any motion); moving = the motion
  share past half (the older report's "moving"). A still head is the eye's view turning under 2 degrees and moving
  under 3 cm a second; "classed moving" is the share of those pairs kept less than the still share. Over 20%: the
  thresholds are wrong, not the blend (`thresholds_wrong`).
- **Eye depth log:** for the first 200 pairs, the fmt 19 (R32G8X24_TYPELESS) / 20 depth at the eye's size that leaves
  DEPTH_WRITE inside each eye job's G-buffer pass: which resource each eye used, whether both eyes' jobs had one,
  whether the same one served both, and whether it ever left DEPTH_WRITE with no eye job open (the flat view).
  CONFIRMED only with 200 pairs, at least 90% with both eyes and never the flat view; otherwise CANDIDATE.

### TAA JITTER / TAA STILL (0.0.76 A/B/C tests): far windows that swim with a still head

The owner: with the head completely still, medium-to-far windows and thin lines swim or shift slightly (they do not
vanish); close edges are usually fine; while swinging, closer windows swim too (the motion case: A-depth's item).
Sub-pixel detail at a distance, so two live.txt diagnostics, one line each, live, never kept, nothing changes without
them:

| live.txt | Default | What it does |
| --- | --- | --- |
| `vr_taa_jitter` | `on` | Our own Halton (2, 3) lens step only (include/spidy/taa_jitter.hpp; game_xr's eye_taa::step): `on` the 16-step walk; `off` no step (the lens, the history's reprojection views and the headset layer all unjittered); `fixed` one constant offset (the walk's second step, -0.25 / +0.17 pixel); `half` the same 16-step walk at half size. SPIDY TAA, each eye's history and the game's own jitter (`vr_jitter`, `vr_game_jitter`) are untouched |
| `vr_taa_still` | unset (the build's: 0.9; C-still094 0.94) | SPIDY TAA's still share only, 0.60-0.99 (include/spidy/taa_still.hpp): each pair's motion share is read from the build's own keep and eased from the new still share to the unchanged moving 0.6. The clip, the camera-cut reset and the classification are unchanged |

TAA WATCH, every 10 s, adds the **history rejection**: SPIDY TAA's shader counts, at one pixel in 16 x 16 per eye,
the pixels with a history to blend and those the YCoCg variance clip had to move (a read-only TAA_COUNT variant of
the same shader, its counts copied back per capture slot; if the variant cannot be built the counts read "-" and the
TAA is the one before). Split by a still head (taa_watch::stillHead) vs moving: `rejected still <%> / moving <%>`,
plus `jitter <modes>` and `still share <x>` when not the defaults; the report's `taa_watch` windows have
`rejected_still_pct`, `rejected_moving_pct`, `jitter`, `still_share`. A high still-head rejection that drops with
`vr_taa_jitter = off` or `half` points at the step against far sub-pixel detail; one that stays points elsewhere.
In A-depth the depth pass has its own counted variant (TAA_DEPTH + TAA_COUNT, the depth root signature plus the
same root UAV at u1): a pixel whose history the depth check refuses (disocclusion) counts as rejected, so A's
`rejected` covers both the depth refusals and the clip. If that variant cannot be built, A-depth runs as before and
those pairs go uncounted (the head-turn-only pairs are still counted).

**JITTER CHECK (read in code, then logged).** The question: does the reprojection use jittered or unjittered views,
and is our step counted exactly once in the history's UV? In code: game_xr moves both eyes' lenses by the step
(eye_taa::lensShift / shiftLens on motion.fovs) before noteEyeViews, so each pair's noted view - and the game's
render, and the headset layer's fov - carries that pair's own jittered lens. drawHistory turns the history with
eye_history::historyReprojection(history.shown, now): the current pixel's direction from this pair's jittered lens,
looked up in the last pair's jittered lens, the one the history picture was written in (the history is kept on the
jittered grid it was drawn on; `shown` is the last pair's `now`). So it is counted **exactly once**: with a still
head the history moves by this step minus last (x: now - last, y: last - now, eye pixels), not twice, not zero.
core_tests proves it with the real functions (0.0.76 TAA JITTER / TAA STILL). Not a double count - but one thing the
check does point at: the TAA's output stays on the jittered grid (the headset is handed the moved lens to undo it),
so with a still head the history is re-read with Catmull-Rom at a different sub-pixel offset every pair and the
compositor resamples a picture that moves by the step - two resamples a pair that standard TAA (an unjittered
output) does not have. Far sub-pixel detail is where that shows; `vr_taa_jitter = fixed` (no change pair to pair)
takes it away and `half` halves it, so those two runs tell. Logged every 10 s (head still, eye 0): `still step now
(x, y) last (x, y) px, history shift (x, y) px, step counted once (off by none/once/twice a/b/c px, n pairs)` - the
shift the pass actually used, and the mean distance from each count (head motion under the still limit adds the same
to all three, so the least says how it was counted). Report: `jitter_now_px`, `jitter_before_px`, `shift_px`,
`jitter_counted`, `jitter_off_px`, `jitter_check_pairs`.

**TAA FRAME LOG (jitter diagnostic build, read-only).** include/spidy/taa_frame_log.hpp, `SpidyTaaFrameLogData`,
tools/taa_frame_log_record.py. Eye 0, one entry for each pair through the history pass, the last 300 kept: the view
the pair was rendered with and the view the history was fetched from (each the camera-to-world matrix and the lens
tangents l, r, d, u, which together are the view-projection; our step is already in the lens), our step on each
(eye pixels, x right, y up), the history UV of the eye's centre pixel and its shift (eye pixels, x right, y down), the
shift with the step's change taken out (the drift: shift minus (now - last, last - now)), the shift's change from the
pair before, the head's turn and travel, and a still flag (taa_watch::stillHead). Console, every 10 s with the TAA
WATCH window: `TAA FRAME LOG (eye 0, last 300 pairs, jitter <modes>): <n> head still - drift with the step taken out
mean <a> / max <b> px still (all <c> / <d> px), history UV step still mean / max, measured step gain <g> (once)`, the
gain the share of the step's change the history shift carries (+1 once, +2 twice, 0 none, -1 the wrong sign). Once at
the start, and again at the end, the one line worked out from the code (the JITTER CHECK above):

    matrices: jittered, jitter applied: once, sign: +

The report's `taa_frame_log` has `most_still` (the 300-pair block with the most still-head pairs seen in the run) and
`last`, each with every frame and the summary (`drift_still_mean_px`, `drift_still_max_px`, `measured_gain`, ...).
A still-head drift near 0 with the step removed means the history follows the step exactly; a drift that stays while
`vr_taa_jitter = fixed` or `half` is set points away from the step.

## SHADOW STABLE + TEXTURE DETAIL (TEST 0.0.76): steadier far building shadows, more surface detail

| switch (live.txt) | default | what it does |
| --- | --- | --- |
| `vr_shadow_stable` (live, no restart) | `off` | `on` (1): the far building shadows are made once a frame from your head's view and shared by both eyes, instead of the left eye making them again (CANDIDATE cause of the flicker: the two remakes race and the right eye takes whichever came last); the far cache then behaves as `far_shadows` 1 (EYES); only changes anything with `far_shadows` at eyes+cache (2, the default) |
| `vr_texture_detail` (RESTART, next VR start) | `off` | `aniso` (2): 16x anisotropic filtering on the game's material textures it left trilinear (about 15 of 519 samplers in the visual reports; the rest already 16x), the game's mip bias kept; `quality` (3): Texture Quality Very High while VR runs (yours put back when the game closes; more video memory - about 10 of 15 GB were in use at High); `on` (1): both |

No sharpening, no SMAA, no TAA change. `vr_ss = 125` stays an optional extra.

## AO ON + ONE PAGE (0.0.76): Ambient Occlusion on by default, one wrist settings page

| switch (live.txt) | default | what it does |
| --- | --- | --- |
| `vr_ao` (RESTART, next VR start) | `off` (since the 0.0.76 HARLEYS fix: XeGTAO shimmered in the eyes) | `on` (1): the game's Ambient Occlusion at XeGTAO (AmbientOcclusionQuality 3, CONFIRMED) while VR runs if yours is Off, put back when the game closes; soft contact shading that gives buildings and streets depth, about 1-2 ms a frame (CANDIDATE, not measured); `off` (0): yours as set |

The wrist hologram's VR SETTINGS and GRAPHICS pages are one page, VR SETTINGS (it scrolls): RECALIBRATE, FPS, FPS
HIGH/LOW/AVG + RESET, then BODY AND VIEW, WEBS, HOLOGRAM, MIRROR, SOUND, GRAPHICS and PERFORMANCE. The test and
diagnostic rows (anti-aliasing, sharpness, history, supersample, eye AA, trail, TAA jitter, eye test, address
finder, foveated debug, eye LOD, shared LOD, lens flares, world log) are off the hologram; each is still set by
live.txt. No TAA change, no sharpening.
