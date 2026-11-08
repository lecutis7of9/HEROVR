# Spider-Man 2 Engine TAA / Upscaling — isolated experiment

**Status:** branch and investigation plan only. This is not yet an engine-upscaled working VR build.

## Branch and baseline safeguards

- Experiment branch: `experiment/078-engine-upscaling`.
- Base: public HERO VR v0.0.78 release commit `4c2b2763a36ab04bbd7063b505a9e2591f56470b`.
- The successful custom temporal-upscale branch `experiment/078-temporal-upscale` at `21f9e2ef3a321885ef23110e0f66ad9029ae1231` is **not** changed.
- The temporal-detail branch and AER branch remain independent.
- First experiments should be gated and non-default; no experimental native TAA is silently enabled for players.

## Goal and terminology

SpidyVR in Spider-Man Remastered uses the **game's own temporal anti-aliasing (TAA)** to resolve smaller per-eye scenes to headset-sized images. This is distinct from enabling the game's DLSS/FSR upscaling path.

This branch asks whether Spider-Man 2's engine TAA can similarly reconstruct two independent VR eyes at reduced render size with stable stereo results. Simply enabling TAA is **not proof of an actual upscale**.

## Code paths to inspect

- `src/stereo_probe.cpp::initBuffers()`, `engineTaaBuffers()`, `sizeBuffers()`: verify the offscreen eye targets, render size, resolved image size, and whether TAA targets are actually allocated.
- `src/stereo_probe.cpp::setupDisplay()`: ensure each eye's temporal AA command is queued when appropriate.
- `src/stereo_probe.cpp::copyJob()`: inspect the per-eye render job's TAA mode and how it's copied to engine context.
- `src/stereo_probe.cpp::temporalAa()`: trace early-outs, input/output dimensions, jitter, and whether each eye has independent history.
- `src/stereo_probe.cpp::eyeJitter()` and view setup in `placeEyes()`: verify projection jitter applies to eye source rendering and matches the temporal pass.
- `src/native_eye_gpu.cpp::stagePair()`: verify what GPU image is ultimately staged/presented and ensure an engine-resolved image is not unexpectedly resized a second time.
- Compare with upstream `IlyaMez/SpidyVR` `src/stereo_probe.cpp` (`initBuffers`, `sizeBuffers`, `setupDisplay`, `copyJob`, `eyeJitter`).

## Step-by-step engineering approach

1. **Read-only audit first:** instrument for each eye's source width/height, destination width/height, allocation of TAA targets, per-eye job AA selection, queued temporal command and actual temporal dispatch.
2. **Verify the exact return/skip condition:** determine whether the engine's temporal pass never runs, runs but exits early, or runs at only one resolution. Capture the reason in machine-readable telemetry.
3. **Check independent history:** prove previous image/depth and frame stamps belong to the same eye, not the main view or the other eye.
4. **Validate jitter and output ownership:** the jitter must match the eye's actual reduced render dimensions, and the eye output must come from the engine-resolved target.
5. **Enable limited 70%-to-100% engine reconstruction** only once the above conditions hold, under an explicit opt-in, with a fail-safe return to the ordinary 0.0.78 pipeline.
6. **Windows CI + tests:** validate that legacy/native behavior is unchanged when the experiment is off; also check shader/resource bounds and target dimensions.
7. **Headset comparison:** same game scene and headset settings versus the preserved approximately 56 FPS custom temporal-upscale baseline. Assess frame pacing, FPS, close/medium detail, distant window stability, ghosting, and stereo comfort.

## Go/no-go criteria

- No second-eye bleed, black/invalid eye frame, memory hazards, or stale eye histories.
- Verifiable full-resolution engine output from reduced-resolution eye input on both eyes.
- Performance and clarity must be measured before recommending this branch.
- Only a working and reversible mode belongs in the launcher. Do not claim DLSS/FSR works from a TAA-only result.

Current conclusion: **feasible hypothesis, not proven in Spider-Man 2**.
