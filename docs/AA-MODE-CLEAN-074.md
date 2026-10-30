# AA MODE CLEAN (0.0.74 test 4)

Made by Agent Mulder.

`aa_mode = clean` is ON by default. `aa_mode = off` gives you back your own AA options (SPIDYVR STOCK,
SPIDYVR PICTURE and MILES TAA as you set them).

It follows how a clean private SM2VR build (0.0.190) handles anti-aliasing. That build was used as a reference
only. No file from it is in HERO VR: every part below is a HERO VR setting that already existed.

## What CLEAN runs

| Part | Setting it uses | Status |
|---|---|---|
| No game TAA in the eyes | image_style = off, taa_jitter = 3 (none) | in our code |
| Eyes at 100% | the eye supersample stays yours (100 by default); no MILES 67% | in our code |
| FXAA on every eye frame | aa = 1 | in our code |
| Per-eye history every frame | history = 80, eye_temporal = 1: head-pose reprojection, 3 x 3 clamp, reset on a cut | in our code |
| Light CAS | sharpen = 40, sharpen_match = off | in our code |
| Mip bias +0.5 with 16x AF | only the samplers the game biases for its upscaler (mip_bias.hpp) | in our code |
| Flat-game LOD distances | already the default (no --eye-lens-distances) | in our code |
| Building reach 150% | reach = 150 (impostor queries +0x88) | CONFIRMED |
| Left eye picks far buildings for both | impostors = 1 (isActiveView at the five impostor calls) | CONFIRMED |
| Lens flares off | registry LensFlares = 0 for the session | key CONFIRMED |
| Screen shake off | registry ScreenShake = 0 for the session | key CONFIRMED (string in Spider-Man2.exe); value 0 = off CANDIDATE |

The registry values are backed up when VR starts and put back when it closes.

## Not done

- Depth reprojection in the history: CANDIDATE. Our history reprojects by head pose only.
- View flag 32 (the 3-view render): never used.

## Switches and defaults

| Switch | Default | Needs a restart? |
|---|---|---|
| aa_mode | CLEAN | live; eye size, textures and screen shake at the next VR start |
| miles_taa | OFF (kept as an option, works with aa_mode = off) | eye size at restart |
| eye_taa_requeue | OFF | live |
| main_view_small | OFF | restart |
| taa_gate_log | ON (diagnostics) | live |

## Fallbacks

- If live.txt can't be read, you get the default: CLEAN.
- If an export is missing, the launcher skips that value. It does not stop.
- No address is written that is not CONFIRMED.
