# 0.0.73 test 4: IMAGE STYLE, shown as GAME TAA (GRAPHICS > QUALITY, under IMAGE and AA)

**GAME TAA: SPIDYVR (default) / OFF.** Subtext: "SPIDYVR: the game's TAA in each eye as SpidyVR; OFF: IMAGE's look;
no restart." Live (wrist, `live.txt image_style = spidyvr | off`; `hero_0.0.73`/`classic` are accepted for OFF).
Export SpidyImageStyleOption (5460); stereo_probe SpidyImageStyle (4605).

It sits beside **IMAGE** (BUILD-SM2.md section 55, combat38 `image`, SpidyImage 5449: SPIDYVR's sharp EYE AA
history / HERO VR CLASSIC), which landed on hero-0.0.73 in parallel. IMAGE keeps choosing aa, sharpen, the history
read and the mip bias (0 under its SPIDYVR, from the next start); GAME TAA decides whether the game's own TAA
resolves each eye on top. While GAME TAA SPIDYVR has ENGINE TAA on, FXAA, the EYE AA history and CAS stand aside
(SpidyVR sends the TAA result as it is); when it is off or fails, the picture is IMAGE's.

## SpidyVR's picture vs HERO 0.0.73 test 3

| | SpidyVR (Remastered) | HERO 0.0.73 test 3 | IMAGE STYLE SPIDYVR |
|---|---|---|---|
| Anti-aliasing | the game's TemporalAa per eye, own Halton (2,3) jitter | FXAA + EYE AA colour history (88%+) | the game's TemporalAa per eye (ENGINE TAA), jitter OURS |
| Game TAA actually running | yes | **no** - TAA JITTER NONE makes the game's TAA handler return (CONFIRMED, 0x2c2dc60) | yes |
| Extra history | none | 88%+ colour-only (the smudge/crawl) | none |
| Sharpening (CAS) | none (game sharpening lowest) | 140 | none (the slider is kept, not applied) |
| Mip bias | none | +0.5 | 0 (IMAGE SPIDYVR, at start) |
| Render scale | 67% + the game's upscale | 100% | 100% (unchanged) |
| Shadows / exposure | shared between eyes | shared (0.0.73 fixes) | shared (kept) |

Streetlight flicker: SpidyVR's fix is the game's TAA with a real per-eye jitter (the history converges the
sub-pixel lights); test 3 had no game TAA (NONE), so the flicker stayed. SPIDYVR turns it on.

## How it never fails flat

`include/spidy/image_style.hpp` (Controller, run about once a second in `game_xr.cpp`): ENGINE TAA goes on by itself
when nothing blocks it (`SpidyEngineTaaBlocked() == none`); while on, EYE AA is off and `native_gpu::setCleanImage`
skips FXAA/history/CAS. Until then, and on any failure (frame rate halved, targets gone, the game's AA not TAA/DLAA,
-EngineTaa missing), the picture is IMAGE's (EYE AA) with a log line; never raw, never flatscreen. A player's
own ENGINE TAA press is honoured. OFF turns ENGINE TAA off and EYE AA on when switched from SPIDYVR, then leaves
the presses to the player (IMAGE HERO VR CLASSIC + GAME TAA OFF = test 3's picture).

## CANDIDATE (headset check)

- Far buildings with OURS jitter (run341 reported shaking with OURS before; the jitter scale is the knob).
- Streetlights steadied by the game's TAA.
- DLAA (game AA 5-7) path.
- Mip bias 0 vs the game's own ('game').
- 0.0.73 AA: the jitter units are CONFIRMED equal to the game's (Lens stores 2·arg; docs/AA-0.0.73.md).
- Frame rate with the game's TAA per eye vs the removed FXAA/history/CAS passes.
- The game's own sharpening setting (SpidyVR used lowest; untouched).

## Test 4: "GAME TAA SPIDYVR engaged for about 100 frames, twice" (October 28)

- CONFIRMED (test 4 console, lines 70-74): ENGINE TAA was ON for about 3 s at about 103 s and again at about
  115 s. Each time the next line was "ENGINE TAA: OFF" with no reason. That is reason none: an explicit off
  (an ENGINE TAA press, an EYE AA press while on, or GAME TAA -> OFF). It was not the frame-rate guard (slow), not
  targetsGone and not vrEnded, and refused was 0. IMAGE preset presses were logged nearby.
- CONFIRMED (code): after any off it did not cause itself, the Controller set gaveUp_ for the rest of the session.
  It also never auto-engaged from the start of gameplay until about 103 s, although image_style was spidyvr.
- CANDIDATE: a transient "unavailable" (gameAa / notPrepared) read once at eye creation was taken as permanent.
- Fix (test 5): an off by a press holds while the same eyes live, and ENGINE TAA comes back with new eyes (after
  a menu or a cutscene). targetsGone does the same. An "unavailable" give-up is retried when the block clears.
  slow (frame rate halved) stays off for the session. GAME TAA OFF never tries. The console now says which press
  turned it off ("ENGINE TAA: off by the EYE AA press" / "by the ENGINE TAA press" / "by GAME TAA OFF"). The
  Controller's notes go to the console and the export, not only to the debugger. Test: core_tests "0.0.73 test 5
  GAME TAA SPIDYVR".

## 0.0.73 release: GAME TAA starts OFF

- On test 6 the picture shook in motion with GAME TAA SPIDYVR and was steady with it OFF, so the public 0.0.73 starts with GAME TAA **OFF** (live.txt `image_style = off`, `image_style::defaultStyle`, game_xr's start value).
- SPIDYVR keeps index 0, so live.txt numbers and the wrist mean the same as before. It is still one wrist press away (GRAPHICS > QUALITY > GAME TAA), or `image_style = spidyvr` in live.txt.
- Migration (`live_settings.migrate_0073`, once, from `run_game_vr.py` before image_style is read): a live.txt written before the marker with `image_style` at the test builds' default (`spidyvr`, `0`, `spidy`, `on`) moves to `off`. A 0.0.72 live.txt has no image_style line and simply gets the new default. Anything chosen after the marker stays.
- GAME AA TAA (AA 2: the game's Anti-Aliasing set to TAA with no upscaler while VR runs) only makes sense when the game's TAA renders each eye, so it stays tied to GAME TAA SPIDYVR (`live_settings.game_aa_taa`). With the OFF default the game's own Anti-Aliasing is left alone. Turning GAME TAA SPIDYVR on and restarting VR brings GAME AA TAA back (KEEP MY GAME AA still opts out).
