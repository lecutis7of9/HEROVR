# 0.0.73 AA: "way too sharp and jagged when i move, too blurry when im still" (after test 5)

The owner after test 5: "the AA, way too sharp and jagged when i move, too blurry when im still. never looks good at
anypoint". He wants it as sharp and clean as SpidyVR.

## What test 5 ran (export-test5: small.json and the console)

- live.txt kept **IMAGE HERO VR CLASSIC**, **sharpen 100** and **history 75**.
- The game's Anti-Aliasing was **DLAA**. The window was 1280x720 with DLSS Quality, so the game rendered at 864x480.
- The eyes were 2688x2880. That is 100% of the headset's recommended size (Quest 3 over Virtual Desktop). The average was 37.5 fps.
- ENGINE TAA (GAME TAA SPIDYVR) was on for 1309 frames, about 35 s. GAME TAA was then pressed OFF. The other ~16 min ran IMAGE CLASSIC through EYE AA.
- Eye history: 17090 of 35182 measured pairs were motion cuts. The mean head travel was 0.141 m per pair. 84253 pairs were sharpened.
- Mip bias +0.5 was applied to the 266 samplers the game biased negatively. The game's most negative bias was −2.0.

## Cause (CONFIRMED from the code and the export)

**Jagged and over-sharp while moving.**
- `eye_history` ramps its cut over 0.005 to 0.06 m (or rad) of travel per pair, and TRAIL defaults to 0.
- At swing speed (0.141 m per pair at 37 fps) the history is cut completely. In test 5 that happened in about half the pairs.
- A cut frame is the raw 100% render plus FXAA. CAS 100 then ran on it at full strength, which sharpened the stair-steps.

**Blurry while still.**
- About 75% of a colour history was kept. In CLASSIC it is read bilinearly and only turned with the head (no motion vectors). Each re-read softens it.
- Mip bias +0.5 replaced the game's negative biases (down to −2.0), so the textures were a mip level or more softer.
- CAS then sharpened the soft history's edges, but not the textures.

## SpidyVR vs HERO VR, line by line

| | SpidyVR (Remastered) | HERO VR test 5 (as run) | HERO VR GAME TAA SPIDYVR |
|---|---|---|---|
| AA | the game's TemporalAa per eye | FXAA + EYE AA colour history | the game's TemporalAa per eye (ENGINE TAA) |
| Jitter | Halton(2,3), arg 2(h−½)/size | none on the lens (EYE AA) | Halton(2,3), arg 2(h−½)/size: **identical (CONFIRMED)** |
| Phases | clamp(ceil(8·ratio²), 8, 32) | n/a | the same, 8 at 100% |
| Extra history | none | 75%, bilinear, rotation-only | none (setCleanImage) |
| CAS | none (game sharpness lowest) | 100 at full, in motion too | none |
| Mip bias | the game's own | +0.5 | +0.5 with CLASSIC (fixed below: 0) |
| Render scale | 67% + the game's TAA upscale | 100% (2688x2880) | 100% (no eye upscale found in SM2) |
| Game AA | TAA | DLAA | needs TAA or DLAA (DLAA path CANDIDATE) |

**Jitter units (CONFIRMED by disassembly, Lens 0x2c75560).**
- The lens stores +0x428 = 2·argX (vaddss) and +0x42c = argY × −2.0. The −2.0 constant is at RVA 0x53197c8.
- So an arg of 2(h−½)/size is stored as 4(h−½)/size. That matches the game camera's own values: test 5's 0.002170 and 0.003858 at 864x480 are 4·0.46875/864 and 4·0.46296/480.
- Ours stored 0.00065 = 4·0.4375/2688 at the eye size.
- The earlier "half units" CANDIDATE is **refuted**. Do not raise `taa_jitter_scale` to 200.

**Console label.** "ENGINE TAA: ... jitter NONE" printed the mode that was asked for (`engineTaaJitterMode`). Under GAME TAA SPIDYVR the mode actually in force is OURS (`image_style::jitterMode`). The record now carries the mode in force.

## The fix (branch hero-0.0.73-aa, local only)

1. **SHARPEN MATCH** (combat38 `sharpen_match`, wrist GRAPHICS > DISPLAY AND DETAIL, live.txt `sharpen_match`, export SpidySharpenMatch 5473).
   - **ON** (the default, live): CAS percent = SHARPEN × the share of the still-head history the last pass kept (`keptMilli`). That is full while still and none while a swing cuts the history.
   - When no history runs (history 0), SHARPEN applies as set.
   - **OFF**: test 5's behaviour.
   - Subtext: "ON sharpens only what the eye history smoothed, none while you move; no restart."
2. **Textures under GAME TAA SPIDYVR.** When -EngineTaa is set and live.txt `image_style` is spidyvr (or unset, the default), the launcher starts the game with texture mip bias **0**, even with IMAGE CLASSIC. That replaces the old default +0.5.
   - Revert with live.txt `image_style = off` or an explicit `-MipBias`.
   - It takes effect when the game starts (restart).
   - It is 0 rather than the game's own (`-MipBias game`, which matches SpidyVR at 67%): the eyes render at 100%, where the game's upscaler bias of down to −2.0 would sparkle. That trade-off is CANDIDATE.
3. **Console and export jitter**: now shows the mode in force (stereo_probe.cpp `engineTaaPublish`).

No new game addresses. Every address used was already CONFIRMED (TemporalAa 0xb1 handler 0x2c2dc60, targets 0x2c98240, queue 0x2d017b3, mask 0x5d65408, game AA 0xa164900, Lens 0x2c75560 with +0x428/+0x42c ×2/×−2).

## CANDIDATE (headset check)

- DLAA (game AA 5-7) under ENGINE TAA versus TAA. SpidyVR's path and engine_taa's main path are TAA.
- The per-eye motion vectors and previous matrices the game's TAA reads, and the eyes' set index.
- Why GAME TAA was pressed OFF after 35 s in test 5. Ask the owner.
- Mip bias 0 versus `game` at 100% eyes.
EOF
cat >> BUILD-SM2.md <<'E'

## 57. Version 0.0.73 AA: SHARPEN MATCH, textures under GAME TAA, the jitter in force (after test 5)

The owner after test 5 said: "way too sharp and jagged when i move, too blurry when im still". Full analysis: docs/AA-0.0.73.md.

**Cause (CONFIRMED).** live.txt kept IMAGE CLASSIC, sharpen 100 and history 75, and GAME TAA was pressed OFF after 35 s.
- **In motion:** the eye history was cut whole in about half the pairs (17090 of 35182). Those frames were the raw render plus FXAA, with CAS 100 on top.
- **When still:** 75% of a bilinear, rotation-only history was kept, and mip bias +0.5 replaced the game's negative biases (down to −2.0).

**Jitter units CONFIRMED correct.** Lens 0x2c75560 stores 2·arg (+0x428) and −2·arg (+0x42c; the −2.0 is at RVA 0x53197c8), the same as the game camera's own values. This refutes §55's CANDIDATE.

**Fixes.** All ship ON and can be switched off:
- **SHARPEN MATCH:** combat38 `sharpenMatch`, export 5473, live. CAS × the history share kept (eye_sharpen.hpp `matchedPercent`).
- **Mip bias 0** under GAME TAA SPIDYVR with -EngineTaa (live_settings `image_mip_bias`). Revert with live.txt `image_style = off` or `-MipBias`. Needs a restart.
- **SpidyEngineTaaData.mode** now records the jitter in force.

## GAME AA TAA (hero-0.0.73-aa2)

The game's own Anti-Aliasing is set to TAA with Upscale Method None while VR runs. This is SpidyVR's path. The player's own setting is put back when the game closes.
- **When:** on by default with -EngineTaa and live.txt `image_style = spidyvr`.
- **Switch off:** the launcher's KEEP MY GAME AA, `-KeepGameAa` or `--keep-game-aa` keeps the player's own. Needs a restart, because the game reads it at launch.
- **Registry:** vr_display `SESSION_OFF['game_aa']` = AA 2, UpscaleMethod 0. It is backed up and restored like the other session values.
  - The encoding is CONFIRMED against the game's log in 31 kept reports: 23 read "TAA / None", and 8 with UpscaleMethod 2 read "DLAA / DLSS".
- **Why (CANDIDATE):** test 5 ran DLAA/DLSS Quality.
  - DLSS biased 266 samplers negative for its 864 x 480 monitor render. The eyes share those samplers at 100%.
  - DLSS was evaluated every frame for viewport 0 only. SL WATCH shows the eyes never pass through Streamline.
- **Flatscreen:** nothing is written unless VR runs with -EngineTaa, so flatscreen play keeps the player's setting.

## Differences from SpidyVR (after this branch)

| | SpidyVR | HERO VR 0.0.73 aa2 default |
|---|---|---|
| Per-eye AA | the game's TemporalAa per eye | the same (ENGINE TAA + GAME TAA SPIDYVR) |
| Game AA setting | TAA | TAA, no upscaler, for the session (GAME AA TAA) |
| Render size | 67% of the eye, the game's TAA upsamples | 100% (2688 x 2880, the runtime's recommended size); no SM2 upscale address is known |
| Extra history / FXAA / CAS | none | none while GAME TAA SPIDYVR is in force; EYE AA keeps them if chosen |
| Mip bias | the game's own | 0 (CANDIDATE vs `game`) |
| Game sharpness | lowest | lowest (Sharpness 1) |
| Jitter | game formula | the same formula; units CONFIRMED |
| Forced TAA flag | +0x40c = 1 | job +0x570 = 1 |
