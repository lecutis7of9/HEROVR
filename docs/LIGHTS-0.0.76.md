# 0.0.76 LIGHTS LOCK - lights through buildings (owner's Batman report)

HERO VR - Made by Agent Mulder.

## The report

Playing as Batman (night, Batmobile out), lights shone through buildings again and could not be turned off.

## The earlier fix

0.0.62 LENS FLARES (CHANGELOG: "headlights and bridge lights through walls at night"): the game's lens flares test
visibility against the monitor view's depth, not each eye's, so in the headset they shine through walls. OFF IN VR
turns the game's `LensFlares` off while VR runs (`vr_display.SESSION_OFF`, put back when VR closes). 0.0.73 made OFF
IN VR the default (`migrate_073t5`), and 0.0.74 test 10 kept it (0f16936). It was a RESTART setting that live.txt /
the hologram row could set back to GAME.

## What the logs show (CONFIRMED from the files, reports-lights-batman-1)

- The run was **0.0.74 (Update 39.7) from a backup folder** (`...\HERO-VR-0.0.72-test12\payload-backup\Update 39.5`),
  not an installed 0.0.75 / 0.0.76.
- The game's own log prints **Lens Flares: Off and Bloom: Off** at every settings dump (7, 19:26-19:47); the only
  changes the player made in the game's menu were Vehicle Detail Medium -> Low and the refresh rate. So the flares
  were off during this session: it was not the Batman hero path, the web removal or the merge.
- The player flipped the hologram's LENS FLARES row to **GAME** (and once more back), and live.txt kept
  `lens_flares = game`. As a RESTART row that changed nothing in the session ("couldn't get rid of it"), and it would
  have brought the flares back at the **next** start - the 0.0.62 symptom for real.
- 0.0.74's SEE-THROUGH depth never captured (captured 0, missed growing to 18684 while the Batmobile was out), so the
  Batmobile overlay drew over buildings in that build. 0.0.75 fixed that capture (c1c47ae, find eye depth by command
  list), and 0.0.75.1 / 0.0.76 carry it.

## The fix (0.0.76)

- **Flares are always off in VR, for every hero and every live.txt value** (`live_settings.lens_flares_off` is always
  True; there is no hero, suit or vehicle input, so a hero switch, the Batmobile or a reset cannot change it). A
  live.txt `lens_flares = game` is ignored in VR with one launcher line; GAME only keeps the flat game's own setting.
- The hologram row reads "Flares stay off in VR for every hero, GAME is for flat play only, no restart." and no
  longer carries the RESTART tag.
- Tests: tests/hero076_lights_tests.py, tests/lights073_tests.py, tests/session_launch_tests.py.

## Still CANDIDATE (not changed)

The overlay's translucent glows (Batcave markers, the Crime GPS arrow) are drawn without the scene depth on purpose
(markers); only textured models are occluded. If lights still show through buildings on 0.0.76 with flares off,
the next step is to log which overlay glow is drawn at that moment.
