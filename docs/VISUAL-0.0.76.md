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
| `vr_aa` | `taa` | the game's AA at TAA with no upscaler (AA 2, UpscaleMethod 0, CONFIRMED in 31 reports, docs/AA-0.0.73.md) so SM1STYLE's per-eye TAA runs; `keep` leaves the player's |
| `vr_textures` | `high` | Texture Quality at least High (2) and Texture Filtering 16x (4), only raised; `keep` leaves them |
| `vr_render` | `native` | SM1STYLE's eyes at 100%, the game's TAA with no upscaling; `upscaled` is SpidyVR's 67% |

And: where the game's AA will not be TAA (vr_aa = keep with DLSS), SM1STYLE keeps the launcher's mip bias (+0.5)
instead of DLSS's -2.0.

Encodings (registry beside the game log's [Settings]): TextureQuality 0 Low (023723), 1 Medium (232025), 2 High
(121329, 141958), 3 Very High (grundy); TextureFiltering 0 Trilinear (023723), 3 8x (121329), 4 16x (232025).

## Still CANDIDATE / not done

- SM2's eye distance scale (EYE LOD) and the texture-request raise: not ported (no CONFIRMED SM2 bytes).
- DLAA per eye: the eyes never pass through Streamline (SL WATCH), so not possible.
- SMAA / other AA encodings, Level of Detail 4 (Very High): never seen beside a log; not offered.
