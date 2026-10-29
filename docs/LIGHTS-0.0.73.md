# HERO VR 0.0.73 lights: flickering light halos and headlight ghosts

The owner after test 4: "street lights still flicker and headlights still ghosting". His video shows steady bulbs and
light pools, but the lens-flare streaks and bloom halos round street and traffic lights strobe on and off, worse when
he pans. Car lights leave ghost trails. Changing image, AA, history and sharpen made no difference. The console shows
each change reached the DLL ("kept in live.txt"), and none of those settings touches bloom or flares.

## What test 4 ran (CONFIRMED, from the export)

- EYE BLOOM COPY: the start defaults line lists eye_bloom = 1.
- The game's Lens Flares On: the game log's settings, with lens_flares = game in the defaults.
- The game's Bloom On.
- Motion Blur 0: the launcher sets MotionBlurScale 0, so the game's motion blur is not the trail.

## Why (exe-confirmed rule; the strobe mechanism is a CANDIDATE)

- The game builds every non-active view's settings with bloom zeroed (0x2c27ec0, test r14b at 0x2c29102;
  see include/spidy/eye_bloom.hpp). The eyes are such views, so the game gives bloom to the active (head) view only.
- EYE BLOOM COPY writes the active view's bloom (+0xd8 of [0xc2cae80]) into each eye's settings. That makes the bloom
  and lens pass run three times a frame (the game view plus two eyes).
- CANDIDATE: that pass keeps one frame-to-frame state, not one per view, so the eyes swap it and the halos strobe.
- CANDIDATE: the game's lens flares test visibility against the monitor view's depth, not each eye's. They streak and
  pop as the head moves, and the flare ghosts double the car lights.
- SpidyVR has no bloom or flare code at all, so its eyes keep the game's rule (no eye bloom).

## The fix (on by default)

- EYE BLOOM starts OFF. It works live: the wrist GRAPHICS page's EYE BLOOM row brings COPY back with one press, no
  restart.
- LENS FLARES starts OFF IN VR. The launcher sets the game's LensFlares to 0 for the session and puts it back when VR
  closes. Choosing GAME on the wrist's LENS FLARES row needs a VR restart.
- An existing live.txt at the old defaults (eye_bloom = 1, lens_flares = game) moves once (migrate_073t5). A value you
  choose after the marker stays.
- Sharpness is unchanged: no blur pass is added and the image settings are untouched.

## What a fresh export should show

- The console line "LENS FLARES: OFF IN VR this session".
- The game log's "Lens Flares: Off" while VR runs.
- The defaults line with eye_bloom = 0.
- At the end, "0.0.73 lights: EYE BLOOM OFF at the end ..." (the report's update37.eye_bloom record).

## After test 4 retest: "EYE BLOOM made no difference" (October 28)

- CONFIRMED (our code): EYE BLOOM OFF returns before any write, so the eyes keep the
  game's own zeroed +0xd8 (0x2c29102). OFF already meant "no eye bloom"; a halo that
  stays with OFF comes from another pass, not +0xd8.
- CONFIRMED (our code, eye_taa.hpp historyInForce/loosenInForce): while EYE AA is on
  (test 4: on at start), history is at least 88% and the clamp gives at least 25%,
  whatever the HISTORY BLEND slider says. The user's 70/40/75 changed nothing. The
  pass reprojects for head rotation only and jitters the eye lens over 16 Halton steps.
  SpidyVR runs none of this (eyeTemporal 0: no jitter, no history).
- CANDIDATE: headlight ghosts are moving cars blended with 88% old frames; strobing
  halos are jittered bright points through the game's bloom/flare threshold plus the
  history pass cutting and restarting while panning (878 restarts in test 4).
- CANDIDATE: judder, 40.5 fps on 90 Hz (each picture held about 2.2 frames, unevenly).
- Exe flare systems (CONFIRMED strings, consumer of LensFlares +0x85 NOT traced):
  bloom lens-flare ghosts/halo, anamorphic flare (CS_AnamorphicLensFlareDownsample),
  material "Lens-Flare Occlusion"/"Disallow Anamorphic Flare", VFX "Lens Flare" sprite
  node (string 0x8888108). No corona, glare or occlusion-query strings. Volumetric fog
  has its own temporal pass (CS_VolumetricFogTemporalAA).

## Headset A/B (one change at a time, same night street with traffic)

Each step: stand still 10 s, then pan slowly left/right 10 s. Note: halos flicker
(yes/no), headlight trails (yes/no), streaks. Keep a change only if it helped.
1. Baseline as installed.
2. Wrist EYE AA OFF (live, no restart).
3. EYE AA back on, EYE AA TRAIL 0 (live).
4. Game menu Lens Flares Off, then restart VR (the log must show "Lens Flares: Off").
5. Game menu Bloom Off, restart VR.
6. Volumetric Lighting / fog lower, restart VR.
7. Headset at 80 Hz (stable 2 frames per picture).

## Test 5: LIGHT CHECKS, GLOW CHECK and SL WATCH (October 28)

Every version has the strobe (0.0.63 too) and no image setting changed it, so the cause must be something that every
version has. Every version makes the eyes as kind-2 views (SpidyVR's design).

- CONFIRMED (exe): the street and traffic light halo and streak sprites (VFX 0x2deecd0) ask
  RequestRenderQuery 0x2c8be20 twice per light in the drawing view's query pool (view+0x1c78). The consumer at
  0x2def7ac..0x2def92c hides a sprite whenever either answer is missing or zero. The pool answers 0 when it is full.
  The view constructor 0x2c98350 clamps a kind-2 view's pool to 0x100 (`mov eax, 0x100` at 0x2c98a41). The game's
  own kind-0 view is not clamped. Bulbs and ground pools are meshes and lit surfaces, not these sprites, so they
  stay stable (as in the video).
- CANDIDATE: 256 slots are too few for the eyes' model queries plus every light at night, so the halos fail and
  restart. That strobes them, and the streak sprites give the headlight trails.
- LIGHT CHECKS (export 5470, wrist "ON lets each eye check every light's glow so halos hold, restart VR to apply."):
  ON is the default. While an eye is being created, the clamp's immediate becomes the game view's own room. That
  is at least 1024 and at most 4096, in whole 256s. The game's bytes are put back straight after. Nothing is
  written if the bytes differ from the expected ones. OFF keeps the game's 256.
- GLOW CHECK (5471, debug, live): GAME (the default), ALWAYS (the sprites drawn untested), HIDDEN (none drawn).
  If ALWAYS stops the strobe, the visibility queries are confirmed as the cause.
- SL WATCH (5472, wrist "Test: logs the game's DLSS camera data for the export, no restart.", ON by default for the
  test build, live): this hooks sl.interposer.dll's slSetConstants, slEvaluateFeature, slSetTag and
  slSetTagForFrame in a normal session (not only when the address finder runs). The hook installs once when SL
  WATCH is switched on and is retried cheaply while ON. It logs, every 60 frames, the frame token (a1: the
  virtual index and the raw dword at +40), the viewport id (a2+32), jitter, mvecScale, near/far/FOV/aspect,
  motionVectorsInvalidValue, the seven flags (depthInverted +444, cameraMotionIncluded +445, motionVectors3D +446,
  reset +447, ortho +448, dilated +449, motionVectorsJittered +450, the last one) and structVersion. Per viewport it
  also counts constants, tag calls, the tag-type mask and evaluates. The record is SLWT v1, 1232 bytes, and the
  export console shows "0.0.73 SL WATCH: ...".

## The deciding question for per-eye DLAA (answered from the exe)

Do the eye renders produce a depth buffer and a motion-vector buffer, and does the game's MV pass run for them?

- CONFIRMED: depth. In the target maker 0x2c6eec0, kind = rdx & 0x7ffff, and the eye mask check
  (0x2c6eeff..0x2c6ef18) is skipped for kind 0 (`test r8,r8; je`). GBuffer/kGBufferDepth (id 0x14) is kind 0 and
  is not special-cased by id, so every buffer set gets one, including the eyes'.
- CONFIRMED: a motion-vector buffer. GBuffer/kGBufferVelocity (id 0x15) is also kind 0 and not special-cased, so
  the eyes' sets have one.
- CONFIRMED: TAA depth history (kTaaDepth0/1, ids 0xa5/0xa6, kind 4) exists only with the -EngineTaa widening
  and game AA 1/5..7. Test 4 showed targets L1 R1.
- CANDIDATE: the half-res velocity family (ids 0x2f, 0x37..0x41, kind 0x4000, inside the eye mask 0x3fcc8) is
  id-gated by settings at 0x2c6f560..0x2c6f5dc.
- CANDIDATE: the MV pass on the eye views. ModelVelocityDepth (render command 0x13, handler 0x2b11150) is queued
  at 0x2b4c1d1 (call 0x2c77810 on [r15]+0x160) in the model submit block 0x2b49e71, only when the model count is
  not 0. Whether it lands on the eye lists is not confirmed. CopyVelocityDepth 0xc1 / CacheVelocityDepth 0xc2 are
  queued at 0xd39b80, 0xd39bb9, 0xd39f26, 0xd39f5f and 0x2b562a5, and TemporalAa 0xb1 at 0x2d017c6. The runtime
  check already exists: SpidyCommandTrace / SpidyCommandData counts per owner [other, game view, left eye,
  right eye] (tools/probe_render_commands.py). Row 0x13 with non-zero eye columns confirms it.
- CANDIDATE: camera-motion MVs rely on each view's previous camera, which the frame function 0x175a060 shifts for
  every view. The EYE CAMERA record measures this.

## Next headset test (night downtown with traffic, pan the head)

1. GLOW CHECK ALWAYS: the strobe should stop (the queries are the cause), or not.
2. LIGHT CHECKS ON vs OFF, each after a VR restart.
3. Export with SL WATCH ON: one viewport or two, the flags, and the frame token per eye.
4. SpidyCommandTrace: cmd 0x13 per eye.
