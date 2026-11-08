# 0.0.78 FPS-only optimization branch

## Scope
- **Base:** public tag `v0.0.78`. Do not merge skyline/LOD diagnostic branches.
- **Target:** investigate a 70–80 FPS average on the reported RTX 4080 system, while preserving the exact current image quality. This is a goal, not a promised outcome for any system.
- **Not in this build:** distant-building clarity changes, window-lighting changes, TAA replacement, resolution scaling, lower graphics settings or disabling existing visual effects.

## First code change: attribution, not a claimed optimization
`include/spidy/perf077.hpp` now has two additional lightweight CPU-only scopes:
- `native renderFrame CPU` around `native_body::renderFrame()`
- `offscreen original CPU` around `originalRenderOffscreen()`

`src/stereo_probe.cpp` records these via the existing `perf077::Scope` accumulator. These are **wall-clock CPU call durations**, not GPU timestamps, and the reported ms/s values are aggregate time per reporting window. The new probes do not change eye resolution, TAA or visual features.

## Performance diagnosis required before a change is called successful
1. Establish **application** FPS, compositor FPS, headset refresh, reprojection state, and GPU frame time where available. A reported 40 FPS may include runtime pacing and cannot establish a GPU bottleneck alone.
2. Compare same camera position, motion and settings against unchanged public 0.0.78. Report median and 95th-percentile frame time, not just peak FPS.
3. Separate CPU hook overhead from GPU passes. If the CPU scopes are negligible, do not optimize those functions solely because they appear in the call stack.
4. Select one source-level bottleneck, implement a reversible change, and compare visual outputs and frame timing. No reduction in rendering resolution or image-quality features is permitted.
5. Only package a release when the Windows build passes and baseline/optimized image-quality comparison is acceptable.

## Status
- Branch created and CPU timing scopes committed.
- No verified FPS improvement yet.
- No Windows build, GPU timing capture, or distributable optimized ZIP yet.
