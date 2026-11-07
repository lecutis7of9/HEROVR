# Spider-Man 2 VR: motion-vector trace (source-grounded)

## Located GPU path

The public SM64-SpiderMan2 mod independently reverse-engineered Spider-Man2.exe's D3D12 G-buffer path: it recognizes G-buffer passes by their render-target layout (linear depth, **motion vectors**, two packed 64-bit material targets, stencil-tested depth) and the **per-view constant buffer containing current camera, current projection, and last-frame projection**. Source: https://github.com/ExtCan/SM64-SpiderMan2#how-mario-gets-into-the-world (README, "How Mario gets into the world"). This is the concrete location to instrument; do not start by changing TAA weights.

## HEROVR inputs

- src/stereo_probe.cpp::placeEyes: reconstructs each eye's Descriptor with Lens(Spider-Man2.exe+0x2c75560) and SetPose(+0x2c76310), then submitEye.
- src/stereo_probe.cpp::copyJob: runs after originalCopyJob; the eye job's render context is settings+0x650. Current diagnostics compare view+0x30 and view+0x560 *translation only*. They do not verify current/previous projection and orientation in the actual G-buffer constant buffer.
- src/stereo_probe.cpp::temporalAa: intercepts +0x2c2dc60; history target selected by +0x2c98290, current target by +0x2c98240. Its global job is [base+0xc2c6af0]. This is downstream of motion-vector generation.

## Required implementation, before changing shader values

1. In a GPU capture of one left and one right eye frame, locate the game's G-buffer pass with a motion-vector render target, using the target layout and view constants described above. Identify the root parameter/CBV and offsets for current view/projection and previous projection; verify against the actual executable version 2.810.0.0, not the other mod's build.
2. For each eye and frame, record the G-buffer constant buffer's current and previous view-projection matrices, eye identity, and frame number. Compare to the eye Descriptor submitted by HEROVR in placeEyes and to that same eye's preceding submitted Descriptor (not the desktop camera or opposite eye).
3. If the G-buffer's previous projection is flat, shared, stale, or the other eye's, patch the *producer of that constant buffer* per eye before draw submission. Do not overwrite a GPU buffer in flight or patch TemporalAa's history pointers: these are different stages.
4. Compare a fixed viewpoint's raw motion-vector target (stationary buildings should be approximately zero except jitter compensation) against TAA output. Only then enable a correction.

## Important limitation

The public mod establishes the existence and location in the rendering *pipeline*, not the exact CBV slot, byte offsets, or HEROVR runtime mismatch. Those require an executable/shader capture. No runtime patch is justified yet.
