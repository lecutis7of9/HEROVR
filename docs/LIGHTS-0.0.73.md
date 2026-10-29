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
