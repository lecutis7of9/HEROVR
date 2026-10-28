# 0.0.73 test 4: IMAGE STYLE, shown as GAME TAA (GRAPHICS > QUALITY, under IMAGE)

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
- Frame rate with the game's TAA per eye vs the removed FXAA/history/CAS passes.
- The game's own sharpening setting (SpidyVR used lowest; untouched).
