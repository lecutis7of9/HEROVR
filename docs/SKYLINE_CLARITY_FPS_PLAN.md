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

## Source trace findings (2026-11-07)

### Critical correction: public default AA is NOT SM1STYLE

`tools/run_game_vr.py::apply_sm1style` first checks `a.vr_aa = live_settings.vr_aa_choice(live_text)`. If `vr_aa != 'game'`, it sets `a.sm1style = False` and calls `apply_vr_aa`. The documented default `vr_aa = ours` selects the mod's own per-eye TAA, with per-eye history and Halton jitter. `vr_aa = smaa` selects SMAA; `vr_aa = game` allows SM1STYLE if enabled. Thus the default public build's good shimmer result must not be attributed to SM1STYLE without checking launch logs. Engine TAA is forced off by `apply_vr_aa` and `apply_sm1style`.

### Actual scene-resolution path

`apply_vr_aa` sets upscale to 100%, render scale 100%, and supersample to 100% unless `vr_ss` is explicitly 110/125/150 (and allowed by other settings). It also chooses the game's mip bias when game AA is TAA; otherwise +0.5. `apply_sm1style` sets `a.upscale` to 100% by default, or 67% only for `vr_render = upscaled`. `src/stereo_probe.cpp::eyeRenderSize` uses the `sm1Percent` selected when buffers are created. Confirm effective dimensions and AA in launch/runtime logs before testing image-quality changes.

### LOD path

`include/spidy/shared_lod.hpp::defaultMode` is `higher`, not OFF. `src/stereo_probe.cpp::viewScale` hooks the native scale calculation and may alter each eye's +0x1b28 multiplier using the active game-view scale. This is a real source-level candidate for distant-detail differences, but not evidence that it causes blur. Inspect runtime mode, hook results and existing LOD/texture controls before changing it.

### Performance data gap

`include/spidy/perf077.hpp` captures VR-loop CPU timing and scoped code paths, not GPU timestamps or compositor reprojection. Do not infer GPU bottleneck from the ~40 FPS figure. Gather both GPU and compositor frame timing in the eventual combined test.

### Immediate test-design correction

Use **the exact public default `vr_aa = ours`** as the protected baseline. Do not substitute SM1STYLE. The first comparison should isolate actual eye resolution and LOD/mip decisions while holding AA fixed. Any scene-buffer resizing requires a restart. Keep the old experimental camera-history changes out of this branch.

## FPS is a co-equal priority, not a later optional step

The user's observed ~40 FPS is about 25 ms per delivered frame. This alone does not distinguish application render throughput from VR runtime pacing, asynchronous reprojection, GPU saturation or CPU bottlenecks. The existing `perf077::Frames` samples VR-loop intervals, **not** a GPU timestamp or a definitive application frame time; do not label it GPU timing.

### FPS diagnostic requirements for the same comparison session

1. Record actual headset refresh rate, Virtual Desktop streaming refresh rate and whether the runtime reports reprojection / synthetic frames. A stable fraction of headset refresh can indicate pacing, but is not proof without compositor data.
2. Capture application CPU frame time, GPU frame time and compositor frame timing using available reliable APIs or an external overlay. Mark a metric **unavailable** rather than estimating it from VR-loop timing.
3. Record real eye buffer width/height and render width/height alongside current `vr_aa`, supersampling, texture quality and LOD mode. Avoid comparisons where more than one variable changes.
4. Compare a fixed rooftop skyline and the same swinging path; record median and high-percentile frame time, plus clarity/shimmer observations. FPS alone conceals frame-time spikes.
5. Classify the bottleneck before making performance changes: GPU-bound => examine resolution and expensive rendering passes; CPU-bound => profile hooks, game thread and draw submission; compositor/pacing-bound => diagnose runtime refresh and reprojection. These are decision paths, **not confirmed diagnoses**.

### Acceptance criteria

A proposed change must improve distant clarity or measured frame time relative to the exact same 0.0.78 settings without returning shimmer or breaking other scenes. A clarity improvement that substantially worsens FPS is not a final fix. No target FPS is asserted without first measuring headset refresh and available performance headroom.
