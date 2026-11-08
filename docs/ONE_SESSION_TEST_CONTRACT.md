# One-session test contract — do not run until the diagnostic package is ready

**Goal:** one rooftop + swing session should be enough to decide which code path to investigate next. This is a test contract, not a claim that instrumentation is complete.

## Preflight (must be satisfied before asking the player to test)
- Confirm actual game executable name, public 0.0.78 binary and launcher settings; verify `vr_aa = ours` or record any override.
- Verify the telemetry script on Windows PowerShell 5.1 and check its GPU-counter availability. The script is read-only; it cannot measure GPU frame milliseconds.
- Add and validate actual GPU frame-time + OpenXR compositor/reprojection metrics if accessible, or explicitly state which external VD/OpenXR overlay values must be captured. Do **not** equate process GPU utilization with frame time.
- Verify native eye buffer sizes and active LOD/mip state from runtime logs or add safe read-only native logging. If unavailable, label them unmeasured.
- Confirm restore/rollback of any changes and a buildable release ZIP. Do not distribute a source-only script as a compiled test build.

## One player session (after preflight)
1. Start the known baseline with existing TAA unchanged, headset refresh rate recorded and Virtual Desktop performance overlay visible.
2. Stay at one fixed rooftop skyline for 30 seconds, then swing a repeatable path for 30 seconds, then street/interior for 30 seconds. Mark phase transitions by wall-clock time.
3. Capture process CPU, GPU engine utilization, memory, application FPS, GPU frame milliseconds, runtime/compositor FPS and reprojection, **where genuinely available**.
4. Only if the diagnostic package supports it safely, make one LOD-only comparison from the same rooftop position, while preserving AA, resolution and other settings.
5. Save output, note which objects are blurry (geometry, windows, textures), and restore any modified settings.

## Interpretation
- Native eye buffer smaller than expected => investigate allocation/resolve first.
- High GPU frame time => profile rendering passes and resolution, do not increase LOD blindly.
- High CPU game frame time => investigate game/VR hooks and submission.
- App FPS locked at a fraction of refresh with compositor synthesis => inspect pacing/reprojection.
- LOD-only change improves buildings => isolate geometry/LOD selection; otherwise investigate mips/texture detail/temporal resolve.
- Missing metric => unknown, **not** evidence for a cause.

## Existing implementation
- `tools/skyline_capture.ps1`: read-only process/Windows GPU-engine telemetry, not verified on a Windows host yet.
- `tools/skyline_lod_compare.ps1`: reversible LOD control; not a compiled diagnostic.
- No complete compiled ZIP or validated GPU/compositor timestamp capture exists yet.
