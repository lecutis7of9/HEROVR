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
| `vr_ss` | `off` | 0.0.76 SPIDY TAA TUNE: `125` / `150` render the eyes that much larger per side, filtered down (Mitchell-Netravali, linear light) before SPIDY TAA; about 1.5x / 2.25x the GPU work, so off by default (509bdc9's run was GPU-bound at 45 new pictures/s) |

And: where the game's AA will not be TAA (vr_aa = keep with DLSS), SM1STYLE keeps the launcher's mip bias (+0.5)
instead of DLSS's -2.0.

Encodings (registry beside the game log's [Settings]): TextureQuality 0 Low (023723), 1 Medium (232025), 2 High
(121329, 141958), 3 Very High (grundy); TextureFiltering 0 Trilinear (023723), 3 8x (121329), 4 16x (232025).

## Still CANDIDATE / not done

- SM2's eye distance scale (EYE LOD) and the texture-request raise: not ported (no CONFIRMED SM2 bytes).
- DLAA per eye: the eyes never pass through Streamline (SL WATCH), so not possible.
- SMAA / other AA encodings, Level of Detail 4 (Very High): never seen beside a log; not offered.

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

### SPIDY TAA TUNE (0.0.76, after 509bdc9: the shake fixed, windows and building detail still crawled)

From the 509bdc9 run (Quest 3 / Virtual Desktop, 2688x2880 per eye): 45.0 new pictures/s (slowest tenth 38.5) on an
89.3 Hz headset, GPU-bound (the headset copy waited 5.7 ms on average); SPIDY TAA 0.46 ms per pair; 6995 pairs
blended, 4 restarts, 2567 pairs (37%) eased to 0.6 by small head motion; the game's mip bias -1.0 on 112 of 526
samplers; 16x anisotropy already on.
- History share 0.94 still (0.06 of the new frame), easing only from 0.012 rad / 0.03 m a pair (was 0.004 / 0.01) to
  0.8 at 0.08 rad / 0.3 m (was 0.6 at 0.05): a slow look around keeps the full average. Cuts unchanged.
- Clip gamma 1.5 still -> 1.0 moving (was 1.25 -> 0.85): less sub-pixel detail clipped back to the raw frame (the
  crawl), still tight while moving (no ghosts).
- The history pictures are R10G10B10A2_UNORM (same size as 8 bits): at 0.94 an 8-bit history cannot make a step under
  1/(255 x 0.06) and sticks; 10 bits is a quarter of that.
- RCAS (AMD FidelityFX CAS's robust form, 4-neighbour cross, lobe limit 0.1875) at 0.15 after the TAA, written to the
  shown eye only (its alpha kept): the history never sees the sharpen. (vr_aa::rcasPixel, `rcasShader`, PSO
  `history.rcas`.) It replaces the history -> eye CopyResource, so no extra copy.
- Mip bias -0.5 (was the game's -1.0) on the samplers the game biased negatively only (spidy/mip_bias.hpp).
- Kept: Halton (2,3) 16 steps scaled per eye by its own lens tangents, Catmull-Rom history, 1/(1+luma) weighting,
  YCoCg variance clip, 16x anisotropy (vr_textures high).
- Not done: SMAA before the TAA (T2x style) - 3 more full-screen passes and a copy per eye, about 1-1.5 ms a pair on a
  GPU-bound run (2-3 fps); SMAA alone (`vr_aa = smaa`) has no history so it cannot shake or ghost, but it only
  smooths geometric edges, not the temporal crawl of sub-pixel window detail. `vr_ss` is off by default for the same
  reason (no headroom).
