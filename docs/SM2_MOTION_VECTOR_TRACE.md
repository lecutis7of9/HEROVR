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


## Exact shader constant layout recovered (2026-11-07)

Source: `ExtCan/SM64-SpiderMan2/src/render/inject_shaders.h`, the independently reversed-engineered `VS_DefaultMaterialGBuffer` camera constants. The game binds a **36 × float4 = 576-byte per-view constant buffer at shader register b0**:

| Float4 rows | Byte offset | Meaning |
|---|---:|---|
| V[0..2] | 0x000..0x02f | Camera basis axes |
| V[3] | 0x030 | Current camera world position (xyz) |
| V[4..7] | 0x040..0x07f | Current camera-relative view-projection matrix |
| V[8..11] | 0x080..0x0bf | Previous camera-relative view-projection matrix |
| V[15] | 0x0f0 | Previous camera world position (xyz) |
| V[28].w | 0x1cc | Flags (bit 12 = orthographic) |
| V[33] | 0x210 | Motion-vector scale |
| V[34] | 0x220 | Inverse render size |
| V[35] | 0x230 | Projection epsilon / sky depth |

The game's G-buffer writes **motion vectors to SV_Target1**, with depth in SV_Target0 and material data in SV_Target2/3. The independently reversed-engineered shader projects current vertex position through V[4..7], and previous vertex position through V[8..11] after subtracting V[15]. Thus it is not sufficient to compare just view+0x30 and view+0x560 translation.

**Concrete verification**: capture the b0 constant buffer bound to the actual *left and right* G-buffer draws. Compare V[3] and V[15] against the eye camera positions already recorded by `copyJob()`; then compare V[4..7] and V[8..11] against matrices generated from the respective current and previous eye poses/projections. If both eyes share V[8..11] or one has the desktop projection, that is a candidate cause of the swimming. Also check V[33] motion scale and V[34] inverse render size for the 70% scene-size vs full-size TAA output.

**What is still missing**: a verified engine CPU address / DX12 root parameter and descriptor binding that populates this shader b0 for HEROVR's Spider-Man2.exe 2.810.0.0. `b0` is a shader register, not a memory address, and should not be patched by guessing an RVA. The source above gives *byte offsets within the shader constant buffer*, not a hook location.
