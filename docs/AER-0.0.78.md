# Alternate Eye Rendering (AER) — isolated experiment

**Status: design only. Not a working/launchable rendering mode.**

## Safe branch boundaries

- Branch: `experiment/078-aer-alternate-eye`.
- Base commit: `4c2b2763a36ab04bbd7063b505a9e2591f56470b`, public 0.0.78.
- This branch does **not** inherit the 56 FPS temporal-upscaling result or the temporal-detail experiment. Neither should be changed while prototyping AER.
- The eventual launcher option must remain unavailable until AER works correctly in the headset. Never ship a toggle that simply suppresses an eye.

## Target design

Alternate **fresh game-eye renders** across successive game-frame generations. At each XR presentation, provide two images matching the target headset poses: one real eye and one eye reconstructed using its own previous real texture, pose/lens and (if verified) depth and motion vectors.

Avoid reusing a stale unwarped image or using the opposite eye's texture as a substitute. The goal is a large **potential** reduction in expensive per-eye rendering on lower-spec PCs, not a guaranteed 2x framerate improvement.

## Existing implementation constraints (verified in repository)

- `src/stereo_probe.cpp::placeEyes()` updates/submits both eye cameras prior to render-job creation. `maintain()` creates both native eye views.
- `copyJob()`, `jobEvent()`, `beginJob()`, `endJob()` and the GPU markers track an eye's job by eye index, serial and generation. Jobs execute asynchronously.
- `include/spidy/eye_pair_state.hpp::EyePairState::ready()` demands **both eyes finished for the identical serial+generation**. A single fresh eye cannot produce a frame under this contract.
- `src/native_eye_gpu.cpp::stagePair()` assumes both source textures and resource states exist, and stages two eye images at once. The bridge caches `stagedEyes[2]`, but those textures are not automatically valid reprojections of an eye missing a render.
- The engine's view retirement bit at `+0x1dae4` excludes a view from *new* jobs. Do **not** toggle the retirement bit every other frame without first proving it preserves the eye's resources, buffers and histories and cannot race in-flight jobs.
- `eye_history::historyReprojection()` uses a rotational 3x3 homography; there is currently no verified per-object motion-vector pipeline for offscreen eye views. Rotation alone cannot correctly account for near-field parallax or moving objects.

## Incremental implementation

1. Add read-only per-eye instrumentation: frame generation, last completed *real* GPU job, source image timestamp, headset presentation deadline and both poses.
2. Introduce a pure, unit-tested AER scheduler: alternating eye indices based on monotonic game generations; never switch eye on worker callbacks, never accept an earlier generation, and reset on scene cuts/discontinuities.
3. Confirm a **selective eye-job scheduling** hook. It must bypass only fresh rendering work while keeping both native view objects alive and correctly sized. Validate it in an isolated diagnostic build, without headset use as a prerequisite.
4. Cache the last *real* per-eye image and render pose after its GPU fence passes. Track real vs synthetic outputs explicitly so successive syntheses do not recursively feed back as ground truth.
5. Implement per-eye reprojection toward the latest XR-predicted pose; add depth/disocclusion handling for translation and moving subjects when sources can be validated.
6. Extend capture/pair logic to accept one fresh real eye plus one valid synthesized eye while presenting a synchronized stereo pair. Fallback to full stereo on invalid tracking, missing pose/history, stale inputs, failed GPU transitions, excessive motion, and scene cuts.
7. Measure frame pacing, latency, image divergence, near-object stereo comfort and actual GPU timing. Only after those pass expose an **AER (Experimental)** launcher choice with restart and automatic fallback.

## Exit criteria before giving a player DLL

- No black-eye output, stale-eye frame locks, cross-eye copies, invalid resource-state transitions, or renderer crashes.
- Correct per-eye source-generation diagnostics: one real eye per game generation, synthetic eye reconstructed for the same headset presentation.
- Stale-eye reprojection accepts and handles depth changes/occlusions, especially during fast head translation and Spider-Man swinging.
- Full-stereo baseline unaffected. AER is opt-in and fully reversible.
- Windows build/tests succeed and headset validation shows usable stereo stability **and** material performance benefits.

This file documents the engineering prerequisites; it does not claim AER is implemented.
