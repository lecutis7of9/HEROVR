# 0.0.73 test 4: IMAGE STYLE (GRAPHICS > QUALITY)

**IMAGE STYLE: SPIDYVR (default) / HERO 0.0.73.** Subtext: "SPIDYVR: the game's own sharp TAA in each eye, as
SpidyVR, no restart." Live (wrist, `live.txt image_style = spidyvr | hero_0.0.73`); only the mip bias follows at the
next start.

## SpidyVR's picture vs HERO 0.0.73 test 3

| | SpidyVR (Remastered) | HERO 0.0.73 test 3 | IMAGE STYLE SPIDYVR |
|---|---|---|---|
| Anti-aliasing | the game's TemporalAa per eye, own Halton (2,3) jitter | FXAA + EYE AA colour history (88%+) | the game's TemporalAa per eye (ENGINE TAA), jitter OURS |
| Game TAA actually running | yes | **no** - TAA JITTER NONE makes the game's TAA handler return (CONFIRMED, 0x2c2dc60) | yes |
| Extra history | none | 88%+ colour-only (the smudge/crawl) | none |
| Sharpening (CAS) | none (game sharpening lowest) | 140 | none (the slider is kept, not applied) |
| Mip bias | none | +0.5 | 0 (`run_game_vr.py`, at start) |
| Render scale | 67% + the game's upscale | 100% | 100% (unchanged) |
| Shadows / exposure | shared between eyes | shared (0.0.73 fixes) | shared (kept) |

Streetlight flicker: SpidyVR's fix is the game's TAA with a real per-eye jitter (the history converges the
sub-pixel lights); test 3 had no game TAA (NONE), so the flicker stayed. SPIDYVR turns it on.

## How it never fails flat

`include/spidy/image_style.hpp` (Controller, run about once a second in `game_xr.cpp`): ENGINE TAA goes on by itself
when nothing blocks it (`SpidyEngineTaaBlocked() == none`); while on, EYE AA is off and `native_gpu::setCleanImage`
skips FXAA/history/CAS. Until then, and on any failure (frame rate halved, targets gone, the game's AA not TAA/DLAA,
-EngineTaa missing), the picture is HERO 0.0.73's (EYE AA) with a log line; never raw, never flatscreen. A player's
own ENGINE TAA press is honoured. HERO 0.0.73 is exactly test 3 (switching to it turns ENGINE TAA off, EYE AA on).

## CANDIDATE (headset check)

- Far buildings with OURS jitter (run341 reported shaking with OURS before; the jitter scale is the knob).
- Streetlights steadied by the game's TAA.
- DLAA (game AA 5-7) path.
- Mip bias 0 vs the game's own ('game').
- Frame rate with the game's TAA per eye vs the removed FXAA/history/CAS passes.
- The game's own sharpening setting (SpidyVR used lowest; untouched).
