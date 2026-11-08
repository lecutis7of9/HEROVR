# Skyline clarity and FPS investigation — 0.0.78 baseline

This branch starts from the public `v0.0.78` tag, not from the previous experimental camera-history branch. Do not merge that branch until its code is reviewed and validated.

## Confirmed from source

1. `include/spidy/sm1style.hpp`: SM1STYLE uses the game's native TemporalAa and per-eye targets. Its older 67% constant is **not proof** that the current build renders at 67%. `madePercent(..., wanted)` uses the launcher's requested percentage (50–100), and `src/stereo_probe.cpp` records the actual `SM1STYLE: eyes made at N%` value. If the feature is not ready, it selects 100%.
2. `src/stereo_probe.cpp::eyeRenderSize()` computes scene dimensions from `sm1Percent` and the actual eye width/height. Inspect this before suggesting an upscale fix.
3. `include/spidy/shared_lod.hpp`: the LOD hook adjusts view +0x1b28 based on the eye and active-game-view detail scale. Its default mode is `higher`; do not assume it is OFF based on older comments. `src/stereo_probe.cpp::viewScale()` is the active hook. Check the runtime setting and mode counters before changing it.
4. `include/spidy/perf077.hpp`: the VR loop logs CPU-side frame timing and three scoped paths. It does **not** measure GPU time, so a 40-FPS result cannot yet be assigned to CPU or GPU.
5. The user's public-build report: near/interior clarity is acceptable and shimmer is mostly controlled, while distant skyline detail and FPS remain poor. Preserve this baseline rather than replacing TAA blindly.

## One-pass offline source work

- Trace launch settings `vr_render`, eye scale, SM1STYLE and any post-sharpening into the actual eye render targets.
- Trace the LOD hook and texture mip-bias paths; distinguish mesh LOD from texture streaming.
- Identify the available GPU and VR compositor frame-timing sources. CPU VR-loop timing alone is insufficient.
- Identify what the window-light correction changes; do not assume it is caused by the same issue as distant blur.
- Avoid introducing new rendering writes until a specific failure mode is evidenced.

## Combined test (only after a build compiles)

Preserve public 0.0.78 as mode A. Include only verified, reversible changes for mode B (LOD/mip) and mode C (resolution/temporal softness), **one variable per comparison**. Capture the effective eye size, scene render size, active LOD mode, FPS/frame timing and VR compositor metrics. Use a fixed rooftop skyline, then swinging, and check for renewed shimmer. If a mode changes eye buffer allocation or history, restart between modes; do not claim a live switch is safe without proof.

## Decision tree

- **LOD/mip changes restore distant detail without FPS loss:** retain the smallest effective change; validate street-level geometry and shimmering.
- **Increasing real scene resolution restores detail but costs FPS:** prioritize GPU-cost analysis and a better quality/performance tradeoff, not an unverified TAA rewrite.
- **Detail is still blurred at full scene resolution:** inspect temporal resolve and post-processing, using captured data; test a reversible sharpness change only if the source path supports it.
- **No clarity improvement from these paths:** investigate texture streaming and the game's distant-building rendering independent of AA.
- **FPS remains low:** compare CPU timing, GPU timing and compositor reprojection; optimize the measured bottleneck.
- **Any regression:** revert the tested change and keep public 0.0.78 as the known-good baseline.

## Status

Source review and branch isolation only. No Windows binaries compiled or headset results collected. Do not describe this as a ready-to-run diagnostic ZIP.
