# SM2 VR (port of SpidyVR): build and test on Windows

This folder is **SM2 VR (port of SpidyVR)**, an early port of **Spidy / SpidyVR by Ilya Mezerowsky**
(https://github.com/IlyaMez/SpidyVR) from Marvel's Spider-Man Remastered to **Marvel's Spider-Man 2**.
It is licensed under the **GNU GPL v3** (see `LICENSE`), like the original. All credit for the VR engine
goes to Ilya Mezerowsky. The Spider-Man 2 addresses come from the SM2VR 0.0.27 mod.

**Phase 1 goal:** the game boots and shows in stereo VR with head tracking. Hand webs, swinging, the
in-game VR settings tab, the full body, the VR HUD, slow motion and button prompts are **not ported yet**.
They are switched off (see the end of this file).

Supported game: **Steam, Spider-Man2.exe version 2.810.0.0**, with SHA-256
`c9b912eff96facb746a705ac8b0b83c29faa7bbe53d984a0c5eb45efd0b738c9`. Any other version is refused on
purpose.

---

## 1. What to install (once)

1. **Visual Studio 2026 Community** (2022 also works). In the Visual Studio Installer, tick the
   **"Desktop development with C++"** workload. It includes the MSVC compiler, the Windows SDK and
   "C++ CMake tools for Windows", so you don't need a separate CMake or Ninja.
2. **Git for Windows** (https://git-scm.com). The bootstrap step downloads four libraries with it.
3. **Python 3.11 or newer** from python.org (not the Microsoft Store one). Tick "Add python.exe to
   PATH" in the installer. The VR session itself is driven by the Python scripts in `tools\`.
4. Your VR setup as usual: **Virtual Desktop** (or another OpenXR runtime) connected to the Quest 3.

## 2. Unzip

Unzip `sm2-port.zip` to a short path without special characters, for example `C:\sm2-port`.
The rest of this guide assumes that path.

## 3. Download the libraries (once)

Open **PowerShell** (Start menu > PowerShell), then run:

```powershell
cd C:\sm2-port
powershell -ExecutionPolicy Bypass -File .\tools\bootstrap.ps1 -Observer
```

Expected output: some git clone messages, then four lines like:

```
OpenXR SDK 1.1.49 verified: 977f6675bc0057d5a54ed290cb5c71c699b1c0ab
Dear ImGui 1.92.9b verified: f1cc2ae15e53a861a874c3034aae6798fde194ab
bHaptics SDK 2.7.1 verified: d016a3f22a4c1e5b6b2192e3ed2224512a045a19
MinHook (master, VEX-capable HDE64) verified: 8af6b4acae5a9388fd742b56fa79ece89d96f823
```

The libraries land in `third_party\`. An existing MinHook 1.3.4 folder is updated in place (you see
"MinHook updated from 1.3.4 to the pinned revision."). If you see a "revision differs" or "local changes"
error, delete that folder in `third_party\` and run the command again.

## 4. Build

In the same PowerShell window:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build.ps1 -Observer
```

`-Observer` is required: it builds the game modules (DLLs). Without it, you only get the launcher and lab.

The first build takes a few minutes. You'll see CMake configure, then lines like
`[123/180] Building CXX object ...`, then the tests:

```
100% tests passed, 0 tests failed out of 3
```

- **Warnings** (lines with `warning C4...`) are expected from the original code and do not stop the build.
- If it ends with **`Build or tests failed`**, scroll up to the first line containing `error`, then copy
  that line and about 10 lines around it when asking for help.
- If it ends with `Visual Studio 2022 or 2026 C++ build tools are required`, the
  "Desktop development with C++" workload is missing. Add it in the Visual Studio Installer.

To rebuild from scratch, delete the `build\` folder and run the build again.

## 5. Where the files land

Everything goes to **`C:\sm2-port\build\windows-ninja\`**. The important files are:

| File | What it is |
|---|---|
| `spidy_stereo_probe.dll` | The VR module: stereo eyes, OpenXR, head tracking (`game_xr` and the rest) |
| `spidy_bridge.dll` | Input bridge: the game's camera and player |
| `spidy_ray_bridge.dll`, `spidy_movement_bridge.dll` | Loaded, but **not started** in this port (they refuse Spider-Man 2) |
| `spidy_render_probe.dll` | Finds the game's graphics queue (Spider-Man2.exe, SM2VR's 0x9016000 code range) |
| `spidy_render_memory.dll` | Gives the game a 512 MB per-frame render ring (SM2VR's allocator 0xc15d780, hooks 0x2c6c110 / 0x2c6bf10) |
| `spidy_launcher.exe` | The launcher window (also runnable from here) |
| `bhaptics_library.dll` | bHaptics' SDK, copied unmodified |

You don't copy any of them into the game folder. The Python tools inject them into the running game
from `build\windows-ninja`.

## 6. Before the first test

- **Remove SM2VR (and any other mod DLL) from the game folder** (`...\steamapps\common\Marvel's Spider-Man 2\`).
  These include `dxgi.dll`, `d3d12.dll`, `dinput8.dll`, `version.dll`, `winmm.dll` and the like. Two VR
  mods at once won't work, and the script refuses or warns when it finds them. Keep a backup somewhere else.
- The game must be the **Steam** version 2.810.0.0. Verify the files in Steam if unsure.
- Start Virtual Desktop and connect the Quest 3 **before** launching.

## 7. Launch and test

**Option A (simplest): the script.** Double-click **`Test Spidy Game VR.cmd`** in `C:\sm2-port`. It
runs a 20-second timed test. To play until you quit, use **`Launch Spidy Game VR.cmd`**.
The script launches the game through Steam (app 2651280) if it isn't running, waits for it,
loads the modules and starts VR.

**Option B: the launcher window.** Run `build\windows-ninja\spidy_launcher.exe`, point it at
`Spider-Man2.exe` if it asks, and press the start button. Its self-update is turned off on purpose,
because SpidyVR's own updates are for Spider-Man Remastered and would replace this port.

**What you should see:**

1. The game starts. Before the session, the script switches the game's ray tracing off (RT reflections,
   shadows, AO, interiors, ray reconstruction) and restores your settings afterwards. This matches
   SM2VR's stability fixes.
2. Load into free roam. The console prints the steps ("looking for the game's graphics queue", "loading
   Spidy's modules into the game", "starting VR").
3. In the headset you should see the game in **stereo 3D, with your head moving the camera**. The
   monitor shows the game's own view, which is always on in this port (SM2VR's other stability fix).
4. Expected: "Render memory: 512 MB for two frames (the game's own: N MB)" once frames run. That
   needs a game started by the launcher, because the ring is replaced ~3 s after the game process starts. A
   "Render memory module: 7001" warning means this game build's code differs, and the game keeps its own
   ring. You will also see a one-time Steam guard notice. The in-game SPIDY VR menu, slow motion and button prompts report "not available" codes
   instead of hooking.
5. Play with the gamepad, or with your arms: from Update 6 the controllers' world rays and web swinging
   are on (grip shoots a web and holding it swings, trigger reels in). See section 15.

**After the test,** the console prints a report path under `reports\`. If something fails, send the
last 30 lines of the console and that report file.

Settings are stored in `%APPDATA%\Spidy`, shared with the original Spidy. If you also use Spidy for
Spider-Man Remastered, its settings and this port's are the same file.

If the game is left with ray tracing off after a crash, run:
`python tools\vr_display.py --restore`

## 8. What is switched off or unfinished (SM2TODO)

Search the source for `SM2TODO` to find each of these. "Off" means it is cleanly skipped. Nothing is
hooked with a Remastered address.

| Feature | State | Where |
|---|---|---|
| Hand rays and web swinging (`spidy_ray_bridge`, `spidy_movement_bridge`) | **On** (`SM2_PORT_GAMEPLAY 1`), SM2VR 0.0.27 values; switches itself off on a byte mismatch (section 15) | `src/native_rays.cpp`, `src/native_movement.cpp`, `src/game_xr.cpp` |
| Web grab of props and thugs, punches (`native_bodies`) | Not hooked (code 8099) | `src/native_bodies.cpp`, `src/game_punch.cpp`, `src/game_swing.cpp` |
| Web shooter gadgets, the game's wall crawl, wall interactions | Not hooked (code 9990) | `src/game_shooter.cpp`, `src/game_warp.cpp`, `src/game_offers.cpp` |
| Hero webs drawn in VR | Not hooked (code 6099) | `src/native_webs.cpp` |
| Full-body pose (arms follow your hands) | Not hooked (code 7099) | `src/native_body.cpp` |
| VR HUD panel and its placement hook | Not hooked (code 9599), `SM2_STEREO_HUD 0` | `src/native_hud.cpp`, `src/stereo_probe.cpp` |
| Resting view's drawn-list marks | `SM2_STEREO_RESTED_MARKS 0` | `src/stereo_probe.cpp` |
| Wider offscreen target kinds (temporal) | `SM2_STEREO_TEMPORAL_KINDS 0` | `src/stereo_probe.cpp` |
| In-game SPIDY VR settings tab | Not hooked (code 9399) | `src/game_menu.cpp` |
| Button prompts help | Not hooked (code 9699) | `src/game_prompts.cpp` |
| Slow motion (game time) | Not hooked (code 9799) | `src/game_time.cpp` |
| Steam ticket guard | Ported from SM2VR 0.0.27 values (section 12) | `src/game_steam.cpp` |
| Hero health (for haptics/effects) | Table address 0, skipped | `src/game_player.cpp` |
| Diagnostic modules (game_view, camera_watch, eye_capture, collision/movement probes, observer) | Still accept only `Spider-Man.exe`, so they refuse Spider-Man 2 with code 1001. None is started on the Phase 1 boot path | `src/*.cpp` |
| Self-update | Off under `SM2_PORT` | `apps/launcher/update.cpp` |

The three `SM2_STEREO_*` switches are guarded by `#error`: setting one to 1 without first filling in the
Spider-Man 2 values stops the build on purpose.

## 9. Licence and credits

- SpidyVR / Spidy © Ilya Mezerowsky, GPL v3. This port is GPL v3 as well; `LICENSE` is unchanged.
- Third-party notices: `THIRD_PARTY_NOTICES.md` (OpenXR SDK, Dear ImGui, MinHook, bHaptics SDK).
- Spider-Man 2 addresses come from the SM2VR 0.0.27 mod's binaries.

## 10. Update 1: render discovery (code 1001)

- `src/render_probe.cpp` now accepts `Spider-Man2.exe`. Its stack filter uses SM2VR's code range,
  0x9016000 (from `lea rax,[r10+0x9016000]` at RVA 0x1318 in SM2VR's `spidy_render_probe.dll`); RS used
  0x8603000. The hooks themselves are runtime DXGI/D3D12 vtable hooks (Present slot 8,
  ExecuteCommandLists slot 10), and they match SM2VR's DLL.
- `tools/run_game_vr.py`: a 1001 refusal from the render probe now stops the launch at once instead of
  retrying for 180 s. The render memory frame gate gives up after 30 s and looks for the queue anyway.
- `src/native_render_memory.cpp` carries SM2VR's values and check bytes, and `RENDER_MEMORY_PORTED = True`.
  In SM2 the allocator's fields start at +0 (RS +0x10), so the overflow flag is at +0x31 (RS +0x41).
- After updating, close any running Spider-Man 2 first: a game process keeps the old modules it loaded.

## 11. Update 2: Input bridge start 1208

Cause: 1208 is 1200 + MinHook status 8 (`MH_ERROR_UNSUPPORTED_FUNCTION`) from creating the first bridge
hook, the input commit at RVA 0x334d950. Its entry is `48 83 ec 38` (sub rsp,0x38) followed by the AVX
instruction `c5 fb 10 81 54 03 00 00` (vmovsd). MinHook 1.3.4's disassembler (HDE64) cannot decode AVX
(VEX) instructions, so MinHook refuses to build the trampoline. The combat camera hook (0x9907c0) would have
failed next for the same reason: MinHook decodes the vmovss at +9 before it stops copying.

SM2VR 0.0.27's `spidy_bridge.dll` hooks the same five addresses with MinHook; the HDE64 it links decodes
VEX. This port does the same with official upstream MinHook, commit
`8af6b4acae5a9388fd742b56fa79ece89d96f823` ("Update and fix Hacker Disassembler Engine"), which decodes
VEX, EVEX and REX2. No addresses changed.

What to do:

1. Close Spider-Man 2. Unzip `sm2-port-update.zip` over `C:\sm2-port` (replace files).
2. Run `powershell -ExecutionPolicy Bypass -File .\tools\bootstrap.ps1 -Observer` again (section 3). It
   updates `third_party\minhook` and ends with the MinHook line shown in section 3.
3. Build again (section 4). If CMake says "third_party/minhook is MinHook 1.3.4", step 2 did not run.
   If the build folder still has the old MinHook objects, delete `build` and build again.
4. Launch as before (section 7).

Bridge start codes (`SpidyStart` in `src/game_bridge.cpp`; `tools/run_game_vr.py` names them):

| Code | Meaning |
|---|---|
| 1001 | Not running in `Spider-Man2.exe`, or the start block from `run_game_vr.py` does not match (version, process, image base) |
| 1002 | The hero passed in is not the player |
| 1003 | A hook target's bytes do not match (wrong game build or another mod patched it) |
| 1100 + s | `MH_Initialize` returned status s |
| 1200 + 20 * h + s | `MH_CreateHook` for hook h returned status s |
| 1300 + 20 * h + s | `MH_EnableHook` for hook h returned status s |
| 1400 + s | `MH_DisableHook` returned status s (`SpidyStop`) |

Hooks h: 0 input commit 0x334d950, 1 input clear 0x31c8c20, 2 digital query 0x31c5060, 3 analog query
0x31c4e40, 4 combat camera update 0x9907c0. MinHook statuses s: -1 unknown, 1 already initialized,
2 not initialized, 3 already created, 4 not created, 5 enabled, 6 disabled, 7 not executable,
8 unsupported function (the target cannot be copied), 9 memory alloc, 10 memory protect,
11 module not found, 12 function not found. Example: 1288 is hook 4 (combat camera), status 8.

## 12. Update 3: game closes after "Gameplay found; waiting for the first eye images"

That line is where game_xr starts the stereo views (SpidyEyeTemporal, SpidyEyeShadows,
SpidyMonitorRate, SpidyStart) and the stereo probe turns its render hooks on. Compared with
the compiled SM2VR 0.0.27 stereo DLL, the port called Spider-Man 2 code with wrong arguments:

- Scene submit 2d01b30: SM2 reads a 5th argument and a 64-bit 4th one. SM2VR's detour
  (+96f0) passes both through; the port dropped the 5th and cut the 4th to a byte, so every
  scene submit after start read stack garbage. Fixed.
- Display setup 2d016d0: a 5th argument as well (SM2VR detour +9180). Fixed.
- Pose 2c76310: three arguments; SM2VR passes 0 in r8 at all its calls. Fixed (0 passed).
- SpidyEyeShadows / SpidyEyeTemporal: SM2VR's exports always store 0 (own shadows per eye,
  no jitter, temporal AA or extra reflection targets) and keep only the mode/size. Same now.
- Flag 32 (no game view) is never sent to SpidyStart; SM2VR always keeps the game's own view
  (device lost, DXGI_ERROR_ACCESS_DENIED). run_game_vr.py already forced it.
- Steam ticket guard: SM2VR's values (context 66c8010, call 2fcd29f, return 2fcd2a8, slot 13)
  replace the Remastered ones, so quitting to the main menu is guarded (was code 9899).

SM2VR has no frontend/menu gate before stereo start, so none is added. Remastered values still
in the source are only inside the switched-off SM2_STEREO_* blocks (section 8).

Reports: run_game_vr.py prints the report path at start, always writes a report even if the
game log or the closing game's reads fail, and prints the path of a "VR did not start" report.
Each launcher run writes reports\game-vr-<yyyyMMdd-HHmmss>.json plus -game.log beside it.

## 13. Update 4: VR at the menu, flat screen once in the world

Report game-vr-20261022-163707: final "No native eye images reached OpenXR within 3 seconds"
(status 5), eye_jobs accepted/latched 5195 but left/right copies, begins and ends 0, gpu
matched_markers 0. The stereo probe's copyJob hook (2d03240) took the eye job from settings
+0x4b0 (Remastered). SM2VR 0.0.27's copyJob detour reads it from settings +0x650, so no eye
job was recorded and none was marked for OpenXR. Fixed to +0x650; the switched-off temporal
write moved to +0x424 as in SM2VR. steam_guard 9802 means the 9 bytes at base+2fcd29f differ
from 48 8b 08 48 8b 01 ff 50 68; SM2VR checks the same bytes and skips the same way.

## 14. Update 5: black lower part and a soft image in the headset

Compared with SM2VR 0.0.27, the stereo probe's buffer-size hook (2c6ebf0) set the eye's sizes
only while the eye's buffers were made, and never the output size (+1b990, +1b994). SM2VR's
detour (94c0) also sets them whenever the game sizes an eye's buffer set again (eye +1e38), and
writes the output size (aligned to 8 when asked). Both are now as in SM2VR. The tone hook
(2c32a70) now also copies the eye's 0x28 bytes at +440 into the job's copy of the view, as
SM2VR's detour (9f50) does. SM2VR's defaults are the port's: --size 0 (the runtime's
recommendation), --upscale 67; with eye temporal AA off (as SM2VR), the scene renders at the
full eye size and --upscale has no effect.

## 15. Update 6: arm swinging and controller world rays

**What is on.** Controller world rays (`spidy_ray_bridge.dll`: the Havok cast hook that lets the
controllers find walls and buildings) and native web swinging (`SpidySwingStart` plus
`spidy_movement_bridge.dll`: the player's mover). The controls are SpidyVR's defaults: **grip** shoots a web
and holding it swings; **trigger** reels in. The swing starts only after the rays started, since it casts
through them.

**Where the values come from.** Every Spider-Man 2 address, check byte and struct offset is from SM2VR 0.0.27's
own `spidy_ray_bridge.dll` and `spidy_movement_bridge.dll`, disassembled. None is guessed. The header comments
of `src/native_rays.cpp` and `src/native_movement.cpp` list each Remastered value next to the SM2 value
that replaces it. Both modules now accept only `Spider-Man2.exe`. Their MinHook is the VEX-capable one from
Update 2. A hook failure reports `1200 + 20 * hook + status` (create) or `1300 + 20 * hook + status` (enable).

**Fail safe.** If a start is refused, VR goes on and the console prints one line, and the feature stays off for
that session. A start can be refused because this game build's bytes differ, the exe is not Spider-Man2.exe,
or MinHook refused a hook:
- `Controller world rays and swinging are off: ... (code); VR continues`, or
- `Native swinging is off: ... (code); VR continues`.

The codes are:

| Code | Meaning |
|---|---|
| 1001 | The configuration or game module was refused |
| 1002 | Ray bytes differ, or the motion exports are missing |
| 1003 | Movement bytes differ |
| 11xx / 12xx / 13xx | MinHook init / create / enable |

**Still off (SM2TODO).** These have no SM2VR evidence for their addresses:
- web grab of props and thugs, and punches (`native_bodies`, code 8099; the swing starts with grab kinds 0)
- web shooter gadgets, the game's wall crawl and its wall interactions (code 9990)

Game-drawn webs (`native_webs`, 6099) also stay off, so the overlay draws the webs. SM2VR 0.0.27's stereo
probe does port them (rope functions at 0xaaac90, 0xaac730, 0xab5180, 0xab2590 and 0xab2740, hand points
at 0x10ebce0, rope stride 0xa18). Their rope-slot offsets are not mapped yet.

`SM2_PORT_GAMEPLAY 0` in `src/game_xr.cpp` turns rays and swinging back off.

## 16. Update 7: VR stopped at the first web; texture filtering

**First web ended VR.** The ray bridge read Havok hit records at the wrong offsets. SM2VR's queryOne
(0x23af-0x248b) reads `[rbp+i*0xa0+X]` with the hit buffer at `rbp-0x10`, so each field sits at X+0x10:
normal +0x10, fraction +0x20, body ID +0x58, filter +0x88, with a stride of 0xa0. Update 6 read the hit's
world position as its normal and found no valid fraction. Every web cast failed with 2104, the swing
turned itself off, and the next swing submit returned 2002. The session treated that as fatal and VR stopped.

**Runtime fail safe.** A failure while playing now only turns off that feature: a refused ray batch, a swing
submit or sample error, or a swing that stopped itself. VR continues, and the console prints
`Native swinging is off: ... (code); VR continues` or `Controller world rays and swinging are off: ...`.

**Texture filtering.** `-Anisotropy` (default 16; 0 keeps the game's) applies to the samplers the game biases
negatively, which are its material textures. For those, trilinear becomes anisotropic, and lower anisotropy
is raised. Comparison (shadow) samplers, min/max samplers and every other sampler stay as the game made them.
The report's `mip_bias` section adds `anisotropic_samplers`, `trilinear_samplers`, `most_anisotropy` and
`raised_samplers`.

The eyes get no anti-aliasing, so fine texture detail can still shimmer. Keep `-RenderScale 100`: on
October 22, 80 made close buildings much more jagged (no anti-aliasing to hide fewer pixels). `-MipBias 0.5`
is calmer and softer.

## 17. Update 8: was the update used?

The October 22 report after Update 7 had exactly Update 6's module hashes (bridge 119c4f81…, stereo probe
404bd61e…, rays b4721ef6…, movement a6c6f1ec…) and Update 6's report fields (no `anisotropy` in `mip_bias`),
and it ended with Update 6's "Tracked swing input: 2002", which Update 7's code no longer has. Neither the
sources nor the tools had been replaced, so the session ran Update 6.

From Update 8, `PORT-VERSION` names the update. The launcher prints it ("SM2 VR: Update 8.") and the
report records it as `port_version`. The launcher also warns about a folder inside the mod that holds an
extracted update, which happens when Windows' "Extract All" makes a folder of its own.

## 18. Update 9: eye anti-aliasing, web strand, flicker evidence

**Eye anti-aliasing (mod side; no game addresses).** The game's own AA (DLAA/TAA) does not run on the eye
views, as in SM2VR, so the eyes were aliased. On each new eye pair, the copy into the mod's staging images
now runs FXAA 3.11 (quality, preset 29: 12 search steps, subpixel 0.5, edge threshold 0.125/0.0625). It
is compiled at runtime with D3DCompile (d3dcompiler_47, shader model 5.0); there is no dxc dependency.
It runs once per new pair (~40/s), not once per headset frame. The overlay (webs, marks) is drawn after
it and is not blurred. Any failure (root signature, compile, PSO, heaps, eye not shader-readable) falls
back to the plain copy and is reported as `timing.eye_aa.fault`.
- `-EyeAA off|fxaa` (default fxaa) → `--eye-aa`, options bit `0x400000` = off.
- live.txt: `aa = 0` / `aa = 1` (export `SpidyEyeAA`).
- GPU cost: timestamps around the pass; `SpidyXrTimingData` version 3 (456 bytes), stage 12 `eye_aa_gpu`.
  The report has `timing.eye_aa` (mode, fault, smoothed_pairs, gpu_ms_mean/max) and the console prints it.
- SMAA 1x is not included: it needs its area/search lookup textures and three passes. `-EyeAA` offers
  off and fxaa only.

**Web strand.** `appendWeb` was already a braided strand, but its braid fades out beyond 14 m and the
1.6-pixel floor kept the rest a solid near-white bar. It now tapers from wrist to anchor and is lit as a
round thread from above. Where the pixel floor widens it past its true width, it is drawn at that share of
opacity (at least 40%), so a far web reads as a thin thread. The native game webs (`native_webs`) stay
off: `web_state` was "failed" in the October 22 report.

**Building flicker: evidence, defaults unchanged.**
- Each eye culls with its own occlusion object (`eye_occlusion` = 2 in 5283 of 5522 samples). SM2VR
  does the same: its DLL calls the occlusion builder `0x2c9b8e0`. So the default stays on.
  `-NoEyeOcclusion` is the A/B test.
- The game's view rendered (`game_monitor_view`), so the game decided impostors and LODs from its active
  view (`impostor_frames` 0 is expected; `-BuildingReach` applies only while that view rests). That
  view was aligned to the head lens, 110.6 × 101.7° (`active_view_aligned` 139555, 0 rejected). In 7639
  of 10728 frames it was placed from a hero position up to 1.02 m from the one the eyes rendered.
- Free commit memory fell from 3.2 GB at start to a median of 974 MB (lowest 201 MB) across 5522
  samples. This can slow streaming.

## 19. Update 10: eye supersampling, web braid, flicker A/B

**Update 9 ran** (October 22 report): `eyes.eye_aa` "fxaa", `timing.eye_aa` fault 0, 10867 smoothed pairs,
0.33 ms GPU mean. FXAA works on edges within one image; it cannot remove moiré.

**Why the moiré stays.** The eye path has no scaling: the game's eye images (2688x2880, the runtime's
recommended size under Virtual Desktop) are copied 1:1 (`CopyResource`) into the staging images and then
into the XR swapchain. The headset's resampling to the panel happens later, in Virtual Desktop and its
compositor. The moiré comes from fine geometry rasterised without the game's temporal AA, which the
eye views do not run (as in SM2VR), and from that later scaling.

**`-Supersample 100..200` (default 100, off).** The game renders each eye at that percentage of the
swapchain size per axis, and every new pair is filtered down to the swapchain size. The filter is a
Mitchell–Netravali filter (B = C = 1/3) applied in linear light, with a radius of 2 output pixels. It
replaces FXAA while it is active. The overlay is drawn afterwards, at swapchain size.
- `launch-game-vr.ps1 -Supersample N` → `--supersample N` → export `SpidyEyeSupersample` (before
  `SpidyXrStart`). It cannot be combined with `-Size`.
- Errors: 3961 means the request was refused because the session had already started. Fault 3806 means
  the filter pass could not be built, so no pair is shown.
- `timing.eye_aa.mode` is 2 ("downsample"). The report now records `supersample`, and its safe path now
  includes `port_version` and `bridge_hash`.
- Cost: the game's pixel count grows by (N/100)². In earlier tests, 125% RenderScale dropped new pairs
  from 45/s to 33/s.
- Measured in a 1-D model of point-sampled fins over 60 periods (1.1–4 output pixels): low-frequency
  alias energy, which is the moiré, fell by 14% at 125, 27% at 150, 40% at 200. A box filter
  was worse than no supersampling at 125 and 150, so the filter is Mitchell, not a box.
- The shader was compiled with dxc (ps/vs 6.0) on the build box. At runtime it is built with D3DCompile.

**Web strand.** The headset shot showed the strand by the hand as a flat white ribbon, for two reasons:
- The vertex step (3 cm + 2.5% of the range) was longer than half a twist (7.5 cm), so the stripes
  fell between vertices.
- The lit core plus the highlight clamped to white.

What changed:
- Where a twist spans at least 6 pixels, the strand has 8 vertices per twist (at most 2400 fine segments).
- The braid's strength follows the twist's size on screen (full at 14 px).
- The stripes are deeper (45%) and run diagonally (`WebLook::helix`).
- The core is greyer, and the highlight is 0.2, so nothing clamps.

A core test covers this.

**Flicker.**
- `final.eye_occlusion` = 2, so each eye culled with its own occlusion object; `-NoEyeOcclusion` was not
  used. That run is the A/B test.
- The game's active view ran up to 1.21 m from the rendered hero position.
- In-game settings that add shimmer to eyes without TAA: Motion Blur, Film Grain, Chromatic Aberration
  and Vignette. Set them off or to 0 for these tests.

## 20. Update 11: comfort settings, eye history, cartoon web, shadow evidence

**Evidence.** The October 23 report and log were from the Update 10 build (`bridge_hash` 7b5c0437…), on the
`-NoEyeOcclusion` launch (`eye_occlusion` 0 in all 1715 samples), at 28.9 pairs/s (lowest 26.5) against an
89.9 Hz headset. The user's verdict: plain `-RenderScale 100` was the only decent, stable launch.
`-Supersample 150` and `-NoEyeOcclusion` cost too much FPS, made the shadows worse and added noise, so they are
no longer recommended. The default launch stays `-RenderScale 100` with no supersampling.

**Comfort settings.** Each session sets Motion Blur, Film Grain, Chromatic Aberration and Vignette to 0 and
restores them afterwards (vr_display.py `PICTURE['comfort']`; `-Keep comfort` leaves them). The registry
names and their encoding come from the player's own October 22 report, which has the game's registry beside
its log's `[Settings]` block (MotionBlurScale 1000 = "Motion Blur 1.000000"). No names were guessed.
The report now records `launch_args` and every `live_changes` entry.

**Eye history (`history` in live.txt, 0–80, default 0).** This blends each eye's previous picture into the
new one at the given share. The previous picture is first reprojected to the new view with a homography from
the two eye poses and FOVs (`eye_history.hpp`), and clamped to the new picture's 3×3 neighbourhood, so it
settles shimmer without ghosting edges. A cut or fast turn (over 0.26 rad) starts the history again.
- Export: `SpidyEyeHistory`. It returns 3962 when refused.
- Report: `SpidyEyeHistoryData` ('SEHD', 64 bytes) gives `eye_history`: percent, blended/restarted/unviewed
  pairs, GPU ms, and fault (1 root, 2 shader, 3 state, 4 views, 5 pictures).
- It adds no game rendering, just one full-screen pass per eye. The cost is in `eye_history.gpu_ms`.
- live.txt is read only once it is saved again after the session starts.

**Mip bias.** This is set when the game creates its samplers, so it is not live. To A/B it, add `-MipBias 0.5`
to the launch.

**Shadows: evidence only, no fix.** Each eye fits and renders its own cascades (stereo_probe.cpp 215–262,
§12). SM2VR's own SpidyEyeShadows/Temporal store 0. Sharing one fit between eyes would need addresses that
SM2VR only evidences for Remastered, not for SM2, so nothing was added. In the reports, the two eyes'
cascade boxes were 13.0 m out of 13.2 m apart and refit per eye every frame. So distant shadow edges land on
different texels in each eye and flicker differently. `-NoEyeOcclusion` widens each eye's view, which coarsens
the shadow texels. That matches the user seeing worse shadows on that launch.

**Web: the 1994 cartoon's rope.** Update 10 lit the strand at five vertices across it, so a strand a few
pixels wide showed only two flat greys. The new look follows the user's reference (the 1994 animated series):
a thick cream rope wrapped in raised coils, cel-shaded and inked.
- `Vertex` gains a `Shape` (x across the strip, coil groups from the wrist, and the sky light against the
  strand's side and view). It stays all zero for every other mark.
- The overlay's pixel shader (d3d12_renderer.cpp, which follows `web_shading.hpp` constant for constant)
  shades every pixel:
  - three flat tones, lit #E8E2D0, a midtone and shadow #B8B4A8, each step ramped over a pixel and a half
  - a dark ink line about 1.6 px wide on the silhouette and around every coil ring
  - groups of 3 raised rings that bulge the silhouette, are lit on the crown and shaded on the far half
  - groups 8.5 cm apart at the hand, spreading 0.16 m per metre along the line
  - two faint lengthwise strand lines between groups where the rope is at least 20 px across
- Rope half width is now 1.3 cm (it was 0.5) and tapers toward the anchor. The pixel floor and far fade
  are kept.
- **Moiré.** Rings under about two pixels long and ink on a rope too thin to carry it fade out, using the
  shape's change per pixel (fwidth), so far webs are clean thin lines.
- **Cost.** About 160 DXIL instructions per web pixel, with no textures. Every other overlay pixel takes the
  plain branch. The strip is 2 vertices per step (it was 5 columns plus up to 2400 fine segments), so it has
  far fewer vertices.
- **Offline render.** `spidy_web_preview <prefix>` (apps/web_preview.cpp; it took `<out.bmp>` before Update 12) draws a held web and four far webs
  at the headset's pixel density (0.00065 rad/px) through the same shading.
- dxc ps/vs 6.0 compile the shader cleanly. A core test checks the flat tones, the ink, the coil bulge, the
  spreading groups and the sub-pixel fade.

## 21. Update 12: the web as a round tube, projection audit

**Web tube.** Each web is now a real 3D tube built on the CPU (src/web_visual.cpp `appendWeb`).
- The cylinder has 8 to 12 sides, chosen by the size on screen. Its coil rings are raised bulges
  (`tubeBulge` in include/spidy/web_shading.hpp).
- Bulges appear only where a ring is over about ten pixels long, and reach full height at twenty. Far webs
  stay thin, smooth tubes.
- Each ring keeps two sets of normals, one from the slope on each side. The edge of a bulge therefore
  shades as a crisp step, not a smear.
- Shading uses the same three cel tones (lit #E8E2D0, a midtone and shadow #B8B4A8) from one
  sky light. Each ring edge carries an ink groove.
- The ink outline is a camera-facing band just behind the tube. It is not an inverted hull: the overlay
  pipeline draws both faces, and the pixel shader has no eye position. The band is dropped when the rope
  is too thin to show it.
- Vertex `w` encodes the kind: tube is 4 + groove, ink band is below -0.5, and the old strand is between
  0.5 and 3.5. The shader has `tube()` and `ink()` branches. dxc vs/ps 5.0 and ps 6.0 compile cleanly.
- **Cost.** A held web is about 18.8k vertices (about 0.8 MB) and about 0.2 ms of CPU time. Back-facing
  quads are culled on the CPU. The GPU triangle load is trivial.
- **Depth, stated honestly.** The overlay has its own depth buffer, not the game's. The tube is per-eye
  world-space geometry, so its stereo and parallax are correct and it reads as round in the headset. It
  is depth-tested only against other overlay items. Game geometry does not hide it.
- **Previews.** `spidy_web_preview <prefix>` writes `<prefix>-front.bmp`, `<prefix>-side.bmp` and
  `<prefix>-stereo.bmp`. They are drawn depth-tested at the headset's pixel density.

**Projection audit (jagged buildings, curved lines).** I traced the path in the code and in report r11 and
found no mismatch.
- **Tracking.** `xrLocateViews` fov is stored per eye (src/xr_session.cpp). The `imageRect` is the full
  swapchain.
- **Submission.** The submitted views use the same frame's tracked pose and fov (src/game_xr.cpp).
- **Rendered eyes.** The rendered eye is a rigid transform of that pose with the fov unchanged
  (src/game_tracking.cpp).
- **Game lens.** The game lens is built per eye from (tan L, tan R, -tan Up, -tan Down). Jitter is zero
  unless temporal AA is on.
- **Report r11.** Render size = recommended size = swapchain (2688x2880), so nothing is resampled. Views
  are `active_view_aligned` with 0 rejected; fov is [110.6, 101.7].
- **Jagged buildings.** The game's TAA is off (FXAA only), which explains the jaggies. Turning eye
  temporal AA on, or raising RenderScale, is the remedy.
- **Curved lines.** The report shows the game's "FOV Correction" at 1.0 (registry FOVCorrection 1000).
  This is a game-side lens-distortion correction. It is the likely cause of straight lines bending, but
  that is **not verified**. Lower it in the game's menu to test. The port does not write it.
- **New telemetry.** The appearance block is v7 (432 bytes). stereo_probe records the lens the game kept
  next to the lens requested. The report shows `eye_lens`, `eye_lens_asked`, `eye_lens_frames` and
  `eye_lens_error`. An error near 0 proves the game used the headset's asymmetric lens as given.

## 22. Update 13: one web width, drawn from the palm

The Update 12 headset shots showed the web tube far too thick next to the hand, and slimming toward the hand
that held it.
- **Cause.** The rope's half width was 1.3 cm, tapered from 1.15x at the start to 0.65x at the end. That is
  about 3 cm across at the hand, and the coil bulges added 37% more, about 4 cm. For a web held between two
  hands the "start" is the lower hand, so the rope thinned toward the upper one. The pixel floor and the ink
  band did not inflate it near the eye: the floor is under the true width nearer than about 7 m, and the ink
  band is 1.6 pixels.
- **Constant width.** Now the rope is 1.3 cm across (`WebLook::radius` .0065 m), the same the whole way, with
  no taper at the hand or the anchor. The coil rings bulge 30% past it (`web_shading::tubeBulge`). The
  1.6-pixel floor still keeps far webs visible, and it only applies where the true width is thinner than
  that. The shot's travelling tip and the anchor splat now scale with the width.
- **Start at the palm.** Webs still leave the web shooter (10.5 cm behind the grip, on the wrist) for aim and
  physics. The overlay is drawn over the hand, though, so a rope from the wrist lay across the palm.
  `webFromGrip` now draws it from the point on its line beside the grip (the palm), at most 15 cm on. A web
  held between two hands runs palm to palm.
- **live.txt `web_width`** sets the width in cm (0.4-4, decimals allowed, default 1.3), for example
  `web_width = 1.0`. It applies from the next image (export `SpidyWebWidth`, in tenths of a millimetre).
- Core tests check the width along the rope from three eye positions, the coil limit, the far floor, a wider
  live width and the palm start.

**Reflections A/B (video at 0:38-0:40).** A near glass building's reflection went flat dark grey as the head
turned. The port already gives the eyes none of the extra reflection targets (section 20; SM2VR has them off
too), so the game's Screen Space Reflections work from incomplete inputs in the eyes. To compare, turn
Graphics > Screen Space Reflections off in the game's menu (saved as ScreenSpaceReflections, which the
launcher reads) and start VR again. The launcher does not change it.

## 23. Update 14: the wrist hologram

A wrist hologram in the style of Ghost of Tabor (`include/spidy/wrist_hud.hpp`, `src/wrist_hud.cpp`). It has no
watch model. A soft blue light sits on top of the left wrist and throws faint rays up to a translucent
navy panel, about 18 x 22 cm, that floats about 9 cm above it and turns to face the eyes. The panel has:

- a red frame with cut corners and a red spider emblem on its top edge
- a blue wireframe Spider-Man
- a red HEALTH bar with its percent
- three gadget rows (icon, name, segmented charges, n/6)
- a round minimap with a red arrow

It is drawn in the overlay's translucent pass after the webs, with the overlay's own stroke font.

**Toggle (the only gesture; the palm-toward-head idea was dropped).**

- Bring the right wrist to the left wrist (wrist to wrist within 11 cm) and pull the right trigger. Do the
  same again to hide it. Nothing else hides it: there is no gaze or timeout.
- The toggle claims the right trigger from the moment it leaves its rest (past 12%) with the wrists
  together until it is let go. During that pull no web, reel, scan or other game action happens. The toggle
  fires when the pull passes 60% while the wrists are still together.
- A trigger pulled anywhere else, or one already held as the wrists meet, works as before.
- A second toggle needs 0.2 s. The panel fades in and out over 0.15 s.
- A short haptic pulse plays on toggle: 0.35 on the toggling hand and 0.2 on the wrist hand.
- The toggle is inactive while the phone, the gadget wheel, the pad layer or calibration is open, and in
  the flat screen.
- `hud_hand = right` mirrors everything: the panel stands on the right wrist, and the left wrist plus the
  left trigger toggle it.

**live.txt.**

| key | values | what it does |
| --- | --- | --- |
| `hud` | 0 or 1, default 1 | 0 hides the hologram and frees the trigger |
| `hud_hand` | `left` (default) or `right` | the wrist it stands on |
| `hud_scale` | 0.5-2, default 1 | its size; a percent such as `120%` also works |

The exports are `SpidyWristHud`, `SpidyWristHudHand` and `SpidyWristHudScale` (percent 50-200). Their
refusal codes are 3965, 3966 and 3967.

**Placeholders.** Everything the panel shows of the game is a placeholder for now, and the panel says DEMO:

- health 100%
- Impact Web, Web Bomb and Trip Mine, each at 6/6 (the names are the game's gadget classes, the counts
  are not)
- a decorative street grid on the minimap; only the minimap's rotation (the headset's heading) is real

All these values live in one struct, `WristHudData`, marked `SM2TODO`. Fill it from the game and set
`demo = false`. No game addresses were added.

**Fail-safe.** The panel's vertices are checked every image (finite, within 1.5 m of the wrist, at most
60000). If that check fails or any exception is thrown, the panel is dropped from that image and the
hologram switches off with a console message, while everything else carries on. To try it again, save
`hud = 0`, then `hud = 1`, in live.txt.

**Preview.** `spidy_hud_preview <out.bmp>` draws two panels over a sky above a stand-in forearm. The second
one has a 60° heading, 40% health and Web Bomb at 2/6.

**Known overlap.** The slow-motion meter also sits on the back of the left wrist, so during slow motion it
may show near the hologram's light.

## 24. Update 15: web shot travel, hologram pages, persistence

**Web shot travel.** When a web is shot, its tip now flies from the palm to the anchor (`WebTravel` in
`include/spidy/web_visual.hpp`) instead of the whole web appearing at once. Only the picture changes: the
swing attaches exactly as before. The live.txt keys are `web_travel` (1 flies, the default; 0 drawn at
once) and `web_speed` (20-2000 m/s, default 180). A flight lasts 0.03-0.11 s whatever the distance.
Exports: SpidyWebTravel, SpidyWebSpeed (Update 23: SpidyWebStart, see section 32; Update 24: SpidyEyeDebug, section 33).

**Hologram pages.** The wrist hologram now has three pages. The ◀ ▶ arrows in the strip under the panel
flip between them, and the strip shows "n/3 NAME".

1. STATUS: health, gadgets and minimap, as in Update 14.
2. VR SETTINGS:
   - RECALIBRATE
   - WEB THICKNESS, 0.4-4 cm
   - WEB SPEED, 60-400 m/s
   - a greyed MIRROR: COMING SOON row (see below)
3. GRAPHICS. Only settings with a live export and a readback are shown:
   - ANTI-ALIASING FXAA/OFF (SpidyEyeAA, read back with eyeAA(), because the probe stores 0 while
     nothing is staged)
   - HISTORY BLEND 0-80 % (SpidyEyeHistory)
   - BUILDING REACH 50-400 % (SpidyImpostorReach, read back with SpidyImpostorReachNow)
   - WINDOW FPS 0-240, where 0 means every frame (SpidyMonitorRate, read back with monitorRateNow())
   - Under an amber "NEXT LAUNCH - RESTART REQUIRED" heading, EYE SUPERSAMPLE 100-200 %. It is never
     applied live: the swapchain is sized at session start, and SpidyEyeSupersample refuses once the
     eyes are running. When it differs from the running value, "RESTART THE GAME TO APPLY" is shown.

Every value shows its number.

What is left out, and why:
- A sharpen pass: none exists in native_eye_gpu, and a new D3D12 pass can't be verified here.
- The game's own Sharpness registry value: its name has no evidence in SM2VR.
- RenderScale: it rebuilds the session.

**Using the pages.** Point the other controller (right when hud_hand=left, mirrored for right) at the
panel, from up to 60 cm away, or touch the panel with its tip (within 2.5 cm).
- A beam and a dot show where it points, and the control under it lights up.
- Pull the trigger to press a button or grab a slider knob; drag while holding.
- From the first touch to the release, the trigger belongs to the menu, so no web, reel or scan fires.
  Pointing at the panel also takes priority over the wrist-to-wrist toggle.
- The menu only answers while the panel is fully shown and the pointing hand is tracked.

**Hard rules, and how they are met.**
- *Clamping.* Every control has one range (`wrist_hud::range`). `clampTo` clamps every value and turns
  NaN into the low end. The apply step clamps again, and each export validates once more.
- *Safe point.* Values are applied in the XR worker's input stage at the start of a frame, before
  anything is submitted. Each one is an atomic store or an export that validates before it stores.
- *Rate limit.* GPU-side settings (AA, history, reach, monitor) apply at most every 0.25 s while a slider
  is dragged, and once more on release (`Applier::due`).
- *Revert.* A refused value goes back to the last good one, and a short red note shows in the arrow
  strip for 3 s. After two refusals the control shows UNAVAILABLE / N/A and ignores input. That disables
  only that one setting.
- *Fault isolation.* An exception in the menu disables the menu only (`wristMenuFault`): the hologram
  falls back to page 1 and the game and VR keep running. A drawing fault still disables only the
  hologram (`wristHudFault`). `hud = 1` in live.txt brings both back.
- *No I/O on the render thread.* The DLL never touches a file. The launcher (a separate Python process)
  reads the values back through `SpidyHudSettings` once a second and writes the files. Each write goes to
  a temporary file that is then renamed into place.
- *No live swapchain or session change.* The only restart-only setting, supersample, is stored for the
  next launch and nothing else.

**Persistence across relaunch.**
- `SpidyHudSettings(3)` returns which settings the pages changed (one bit per `wrist_hud::Control`) and
  clears that set; selectors 0, 1 and 2 return their values. Bit 30 marks a valid reply.
- The launcher writes only the settings that changed into live.txt (`LiveFile.adopt`). The lines naming
  them are replaced and comments are kept; a missing line is appended.
- If the player saved live.txt since the last poll, the launcher waits a second so that the player's
  save is applied first.
- At the next start, the six PERSISTED keys found in live.txt (web_width, web_speed, aa, history, reach,
  monitor) are applied right after the defaults.
- The NEXT LAUNCH supersample goes to `next-launch.txt`. It is used when the game is launched without
  `-Supersample`. Delete the file, or set the slider back to 100, to return to the default.

**Mirror: not shipped (greyed MIRROR: COMING SOON).** Research in SM2VR's evidence:
- The body hooks 0x292b880 (pose writer) and 0x292af50 (toModel) are CPU animation-pose jobs, not draws.
- The renderActor hook can filter actors per view (that is how the hero is hidden). Even so, every extra
  scene view runs the game's whole pipeline: the game's own view costs as much as an eye (5.1 ms of a
  19.6 ms frame, SubmitScene 19206a0), far over a 1-2 ms budget.
- There is no evidence of a hero-only draw list, a transparent-background output, or culling winding
  for a mirrored camera.

What a real mirror would need:
- a secondary scene view with a reflected camera
- renderActor letting only the hero's instance through, with terrain, sky and lighting passes suppressed
- an alpha-cleared target composited into the overlay
- the winding flipped for the reflected view
- a measured cost within the budget

Each of these needs new game-address evidence, which this port does not invent.

**Tests.**
- core:
  - pages, controls, slider steps and restart labelling (including NaN and odd values)
  - the pointer meeting the panel
  - the menu's claims, presses, drags and page flips
  - the applier's clamping, rate limit, revert and disable
  - web travel
- live_settings: decode_hud_settings, write_back, adopt not fighting poll, the start-up apply, and
  next-launch.txt.
- Preview: `spidy_hud_preview` now renders four cells: status ×2, page 2 with the hover, and page 3 with
  a held slider and the restart notice.

## 25. Update 16: subtexts, headset sharpening, FPS readout, coloured reticles

**Subtexts.** Every control on the VR SETTINGS and GRAPHICS pages has one dim, wrapped sentence under it
(`helpText`, src/wrist_hud.cpp; at most two lines, checked with no overlaps by the core tests):

| Control | Subtext |
|---|---|
| FPS readout | New frames per second you're actually getting, versus your headset's refresh rate. |
| RECALIBRATE | Runs the T-pose again to re-measure your height and arm length. |
| WEB THICKNESS | How thick your webs look. Only the look: swinging is the same. |
| WEB SPEED | How fast a shot web flies out to where it sticks; it still grabs at once. |
| RETICLES | Shows where each hand's web will stick. |
| MIRROR: COMING SOON | Not built yet: would put a mirror in front of you so you can see yourself. |
| ANTI-ALIASING | Smooths jagged edges; FXAA is cheap but softens fine detail a little. |
| HISTORY BLEND | Mixes in the last frame to calm shimmer; more is calmer but smears fast motion. |
| BUILDING REACH | How far buildings keep full detail; more costs some frame rate and video memory. |
| WINDOW FPS | Limits desktop window updates; lower leaves more for the headset. 0: no cap. |
| SHARPNESS | Makes textures crisper; too high adds shimmer and halos. |
| EYE SUPERSAMPLE | OFF is fastest; higher calms shimmer but costs much frame rate (needs a restart). |

**Headset sharpening (SHARPNESS, live.txt `sharpen` 0-100, SpidyEyeSharpen).**
- What it is: contrast-adaptive sharpening (CAS; include/spidy/eye_sharpen.hpp is the CPU reference, unit
  tested, and the HLSL in src/native_eye_gpu.cpp follows it).
- Where it runs: in native_eye_gpu after FXAA and the eye history, before the hand overlay.
- Only the headset is sharpened. The pass replaces the copy into the headset images, so it adds no
  full-resolution copy. The desktop mirror keeps copying from the staged eye before sharpening, also with
  no added copy; while sharpening is on, the window does not show the hand overlay.
- 0 skips the pass entirely.
- The shader is compiled once with D3DCompile on first enable. That is a one-time hitch on the XR thread.
  It was checked offline with dxc (vs_6_0/ps_6_0, 0 warnings); fxc does not run on the build box.
- Fail-safe: any failure turns only sharpening off. Failures covered: root signature, compile, pipeline,
  heaps, an unreadable eye, or a headset image that can't be drawn into (nothing is recorded then). The
  eyes fall back to the plain copy, and the slider shows UNAVAILABLE. Nothing goes blank or flat.
- Cost: estimated 0.2-0.4 ms GPU for both eyes. It is measured with timestamps in XrTimingData version
  4 (504 bytes), and the launcher prints a "Headset sharpening:" line.

**FPS readout (VR SETTINGS page).**
- Shows "FPS n / r HZ": new eye pairs per second (a pair counts when the copied serial/generation changed),
  measured against the runtime's display period, smoothed over ~0.75 s, refreshed 4 times a second.
- A ~5 s sparkline sits beside the number.
- Colour: green at ≥90% of the refresh rate, amber at ≥60%, red below; "--" when unknown.
- An "EYE PASSES x MS GPU" line under it sums the AA/history/sharpen timestamps. It reads OFF when no pass
  runs and "--" while unmeasured.
- Lock-free: the readout runs on the XR thread, and the GPU times are per-pass atomics.

**Reticles.**
- Colours: the swing aim reticles are now Spider-Man red (#E23636) for the left hand and blue (#2F6BFF)
  for the right, in linear RGB, in both styles (web_visual aimColors, tech_marks hands).
- A miss (the blocked cross) is the hand's colour at 55% brightness and 85% alpha, instead of a shared
  red. "Air" keeps its lower alpha.
- New VR SETTINGS toggle RETICLES: ON/OFF, live.txt `reticles` 1/0 (default 1), SpidyReticles (3971 refused).
  It is persisted like the others (SpidyHudSettings selector 4, bit 7; change bit 12).
- Off skips only appendAimMarker. The markers' motion, the aim, the grabs and the swings are unchanged.

**Tests.**
- core: CAS, FpsMeter, the graphics page's SHARPNESS and the FPS readout's areas, the reticle hand colours
  and the dimmed miss, and the subtext layout (two lines at most, no overlaps).
- live_settings: sharpen/reticles parse, persist and decode.
- Protocol: timing version 4 and the sharpen line.

## 26. Update 17: fixed panel size, scrolling, HOLOGRAM page, colour themes

**Fixed panel size.**
- Every page is the size of page 1 (STATUS): 18 x 22 cm times HOLOGRAM SCALE.
- When a page's content is taller than that, it scrolls. This applies to VR SETTINGS and GRAPHICS at
  large text sizes, and to HOLOGRAM.
- Content is hard-clipped to the inner area (`clipBand`, src/wrist_hud.cpp), with no fade. A slim
  scrollbar sits on the right.
- The page arrows and the page title bar below the panel never move.
- How to scroll:
  - Right thumbstick (the pointing hand's) while pointing at the panel: up scrolls toward the top. It has
    a 0.25 dead zone and moves 16 cm a second at full tilt. The stick is claimed (zeroed for the game)
    only while the pointer is on the panel.
  - Trigger-drag on empty panel space: the content follows the pointer.
  - Trigger-drag on the scrollbar: grab the thumb, or press the track to jump there and drag.
  - The stick is ignored while a slider or a scroll drag is held.
- Hit-testing uses the scroll offset. A control scrolled out of the view is never hovered or pressed, and
  nothing is pressed through the scrollbar.
- Resizing safety: any change of page, height, scale or text size between frames ("relayout") drops the
  current hover and press, releases a held slider or drag, and re-clamps the scroll. So resizing while
  hovering or dragging never fires the control that slid under the pointer.

**HOLOGRAM page (page 4).** Every setting is live, clamped, persisted to live.txt, and has a one-line subtext.

| Control | Range / choices | Default | live.txt | Subtext |
|---|---|---|---|---|
| HOLOGRAM HEIGHT | 0-25 cm | 9 | `hud_height` (accepts `12 cm`) | How far above your wrist the hologram floats. |
| HOLOGRAM SCALE | 0.5-2x | 1 | `hud_scale` (accepts `120%`) | How big the whole hologram is. |
| TEXT SIZE | 75-175% | 100 | `hud_text` | Bigger text is easier to read; the page scrolls to fit. |
| COLOR THEME | spiderman, contrast, deutan, tritan, mono | spiderman | `hud_theme` | Colour sets for colour-blind players; FPS also says OK/LOW. |
| TEXT COLOR | theme, white, yellow, cyan | theme | `hud_text_color` | The colour of names and values; THEME uses the set's own. |

- TEXT SIZE reflows the VR SETTINGS, GRAPHICS and HOLOGRAM pages. Titles, values and button labels grow,
  and subtexts re-wrap to the row width. The core tests check that no text overflows its row and no rows
  overlap from 75% to 175%. STATUS keeps its fixed layout (it is themed, not text-scaled).
- Colour is never the only cue. The FPS readout says OK / LOW / POOR, buttons and sliders print their
  values, and the restart notice is in words.
- Every theme with every text colour keeps a WCAG contrast of at least 4.5:1 for text on the panel
  (tested).
  - Deutan/protan safe: red/green replaced with blue/orange.
  - Tritan safe: blue/yellow replaced with red/teal.
  - Monochrome: greys only.
  - High contrast: near-black panel with white and yellow.
- live.txt words are also accepted as digits (0-4 / 0-3) and as aliases (spider-man, high_contrast,
  protan, monochrome, grey, colour/color spellings). Out-of-range values are refused and reported, and
  the previous value stays.
- New exports: SpidyWristHudHeight (refused: 3972), SpidyWristHudText (3973), SpidyWristHudTheme (3974),
  SpidyWristHudTextColor (3975). SpidyWristHudScale is unchanged.
- SpidyLiveSettings selector 3 now has control bits 13-17. The new selector 5 packs height (bits 0-4),
  scale % (<<5), text % (<<13), theme (<<21) and text colour (<<24), so the launcher writes changes made
  in VR back to live.txt.
- All values are applied at the input stage (no file I/O on the render thread, no swapchain or session
  changes).

**Fixes.**
- The menu's repeat/note timers used `frame.seconds` (a frame delta) as "now". They now use a running
  hologram clock. Before this, slider auto-repeat and red refusal notes could stick or vanish early.
- The overlay font gained ';'. Subtexts that use it showed a gap before.

**Previews** (`spidy_hud_preview <out.bmp> [pages|scroll|themes]`):
- sm2-hud-pages-preview.png: every page at its defaults.
- sm2-hud-scroll-text-preview.png: GRAPHICS scrolled with the pointer on the scrollbar, then
  VR SETTINGS at 150% text (top and end), and HOLOGRAM at 150% halfway.
- sm2-hud-themes-preview.png: one tile per theme.

**Tests.** Six new core test groups cover reflow, scroll geometry and hit-testing, `clipBand`, theme
contrast, the HOLOGRAM settings, and menu scroll and relayout input. There is also a new live-settings
test for parsing, ranges, persistence and decoding selector 5.

## 27. Update 18: web styles and web colours

**WEB STYLE** (VR SETTINGS, page 2, under WEB SPEED). It is a ◀ NAME ▶ selector with the subtext "Pick how
your webs look; only the look changes, not swinging." It is live and persisted (`web_style` in live.txt).

| Style | Look |
|---|---|
| 90s (default) | Unchanged: the cream rope of Updates 7-17. |
| MESSY | Thin, translucent, silvery. Six fibres that fray out, with short wisps and glints near the hand. Far off, where the rope is under ~6 px wide, it falls back to one tube. |
| MODERN | White/silver with a helical stripe and a gloss highlight. |
| SYMBIOTE | Glossy black with a coloured sheen and a slow wobble. |
| ENERGY | A bright electric core with a soft glow around it. |

- Only the look changes. Swinging, aiming and the web's physics do not.
- Every style uses the existing tube/overlay path (no textures, no new passes). Each one:
  - follows WEB THICKNESS and WEB SPEED;
  - keeps the far minimum-pixel floor and the shot's travel animation;
  - stays inside the overlay's vertex budget (`web_style::vertexBudget`, tested).
- Faults fall back to 90s. If a style's geometry is not sane (non-finite or over budget), the 90s web is
  drawn instead (`web_style::sane`).
- The HUD colour theme does not affect the webs.
- **Where you see them.** The styles are drawn on the mod's own overlay webs. Native webs (the game's own
  web lines) are the default and keep the game's look, so launch with `-OverlayWebs` to see the styles.
  The overlay is also what you get when the game's lines are unavailable (fallback).
- The ENERGY glow is blended with SRC_ALPHA, the same as the other overlay geometry. It is not truly
  additive, so it brightens without blowing out to white.

**WEB COLOR** (directly under WEB STYLE, always shown). It is a ◀ NAME ▶ selector with the subtext "Shade
of the selected web style." Its list depends on the selected style, and each style remembers its own
shade.

| Style | Shades (first is the default) |
|---|---|
| 90s | CREAM, IVORY, PARCHMENT, WARM GREY |
| MESSY | SILVER, PEARL, SMOKE, FROST |
| MODERN | BRIGHT WHITE, PLATINUM, ICE, CHROME |
| SYMBIOTE | BLUE SHEEN, PURPLE SHEEN, CRIMSON SHEEN, MATTE |
| ENERGY | BLUE, RED, GREEN, ORANGE, PURPLE, GOLD |

- Only ENERGY has saturated colours. The other styles' shades are subtle.
- Every shade name belongs to exactly one style.
- Across styles, no two shades are near the same colour. The core tests check that the shown colours are
  at least 0.08 apart in sRGB across styles, and at least 0.03 apart within a style.
- Pressing the left half of the selector steps back; the right half steps forward. It wraps at the
  style's own count.
- Changing WEB STYLE or WEB COLOR never reflows the page (both rows have fixed heights), so the
  resize-safety rules of §26 are unaffected.

**live.txt.**
- `web_style` = 90s, messy, modern, symbiote or energy. It also accepts 0-4 and `nineties`.
- `web_color` = any style's shade word. Because names are unique, the word also picks the style it belongs
  to, so `web_color = gold` sets ENERGY's shade. A digit (0-5) means the shade in the list of the style
  chosen now.
- `web_color_90s`, `web_color_messy`, `web_color_modern`, `web_color_symbiote` and `web_color_energy` each
  take their own style's words or digits.
- Spaces and hyphens in a word are read as underscores (`warm grey`, `bright-white`). The `colour`
  spelling is accepted too.
- Persisted: `web_style` and the five `web_color_<style>` keys. A plain `web_color` line is kept as its
  style's key.
- Unknown words or out-of-range values are reported, and the previous value stays.

**Exports.**
- `SpidyWebStyle(0-4)`; refused: 3976.
- `SpidyWebColor(code)`: bits 0-3 are the shade, and bits 4-7 are the style + 1 (0 means the style chosen
  now). Refused: 3977 (unknown style, or a shade past that style's list). A refusal shows the hologram's
  red note and reverts the value.
- `SpidyHudSettings`:
  - selector 3 gains control bits 18 (WEB STYLE) and 19 (WEB COLOR);
  - the new selector 6 packs the style (bits 0-2) and each style's shade (3 bits each from bit 3, in the
    order 90s, MESSY, MODERN, SYMBIOTE, ENERGY), so the launcher writes changes made in VR back to
    live.txt.
- Values are applied at the input stage, as before. There is no file I/O on the render thread and no
  swapchain or session changes.

**Previews.**
- `spidy_web_preview <prefix> styles`, saved as sm2-web-styles-preview.png. Rows are the styles
  (90s, MESSY, MODERN, SYMBIOTE, ENERGY), each in its default shade. Columns show the held web seen from
  the head, the held web seen from beside its first metre, and webs across the street 10-60 m away.
- `spidy_web_preview <prefix> shades`, saved as sm2-web-colors-preview.png. It is a grid of
  styles (rows) by shades (columns), zoomed in; empty tiles are shades a style does not have.
- `spidy_hud_preview <out.bmp> webstyle`, saved as sm2-hud-page2-webstyle-preview.png. It shows
  VR SETTINGS with both rows, at 90s CREAM, MESSY FROST, SYMBIOTE CRIMSON SHEEN (pointer on WEB COLOR) and
  ENERGY GOLD.

**Tests.**
- New core tests:
  - style/shade names and unique keys, plus the colour-distance check;
  - geometry for every style × shade × width × flight (finite, within budget, width-scaled, far floor,
    MESSY's far fallback);
  - fault fallback and `sane()`;
  - the hologram's per-style shade memory, labels, subtext and stepping in both directions.
- Two page-2 layout tests were relaxed for the longer page:
  - at 75% text it only has to scroll less than at 100%;
  - a hidden spot may land on the page arrows.
- New live-settings tests cover the names (matched against src/web_visual.cpp), parsing and spellings,
  the export codes, selector 3/6 decoding and persistence.

## 28. Update 19: the MUSIC page (wrist hologram page 5/5)

**Where the songs go.** Put songs in the **Music** folder in the install folder, beside `Launch Spidy VR.cmd`
(typically `C:\sm2-port\Music`). The launcher makes it (with a README.txt) at every start,
`tools\build.ps1` makes it too, and the mod makes it if it is missing. Nothing in it is ever deleted.
The folder is read again when the song list opens and every ~3 s, on its own thread. New files show up
while the game runs. Songs are listed A-Z by file name, up to 5000; sub-folders and hidden files are skipped.
The in-game phone's music player reads the same folder.

**Formats.** MP3, WAV and FLAC are decoded by Windows' own Media Foundation (Windows 10/11). M4A/AAC and WMA
work the same way. There are no extra codecs and no new dependencies (mfplat, mfreadwrite, mfuuid are system
libraries). If Media Foundation is missing (some Windows "N" editions without the Media Feature Pack) the
page shows MUSIC OFF: WINDOWS MEDIA FEATURES MISSING and its controls grey out.

**The page, top to bottom.**
1. Song dropdown ("1/9 SONG NAME"). Point and pull the trigger to open a list of 6 rows; the stick scrolls
   it. A row starts that song. A press beside the list closes it.
2. Visualizer (below).
3. Elapsed time, PLAYING/PAUSED/STOPPED, total time; a progress bar you can point at to jump (or hold to scrub).
4. Buttons: previous song (or back to the start after 3 s in), back 10 s, play/pause, stop, ahead 10 s, next song.
5. MUSIC VOLUME 0-100% (default 70). Only the mod's music: the game's own sound is not changed.
6. VISUALIZER ◀ NAME ▶: ORIGINAL (default), BARS, FRACTAL BLOOM, TUNNEL, STARBURST.
7. MUTE GAME MUSIC: COMING SOON (greyed, SM2TODO, see below).
8. A status line (song count, or notes like SKIPPED: name, NO AUDIO DEVICE) and the folder.
Every control has a one-sentence subtext. The page has a fixed size and scrolls (§26). Nothing on it needs a
restart, so nothing carries a RESTART label. There is no new launch flag.

**Playback.** Its own threads (a scanner, and a player: Media Foundation source reader to WASAPI shared
mode, event-driven, 60 ms buffer). The render and input threads only post commands into a lock-free
queue and read a status copy with try_lock, so they never wait on audio. At the end of a song the next one
plays; after the last it wraps to the first. A file that will not open or breaks while playing is skipped
with a note; if none will play the page says so. A lost audio device (headset unplugged) is retried every
2 s. The phone's player and the page's never play at once: starting one pauses the other.

**Visualizers.** The player feeds a 1024-point FFT (32 log bands with falling peaks, a 64-point waveform,
level, bass) through a lock-free triple buffer. Each visualizer is flat geometry inside its box
(`wrist_hud::appendVisualizer`). The core tests time each one under 0.2 ms (best of 40) and check its vertex
count (30-12000) and sanity, also with NaN data.
- ORIGINAL: red/blue bars mirrored from the middle, with the waveform over them.
- BARS: a classic spectrum with falling peak caps.
- FRACTAL BLOOM: early-2000s style kaleidoscope flowers that branch and rotate with the bass.
- TUNNEL: rings of coloured tiles rushing toward you, faster the louder the music.
- STARBURST: a pulsing core with rays and drifting sparks.

**Fail-safes.** A fault in the player (an access violation in a decoder via SEH, or a C++ exception) turns
only the music off for the session (MUSIC OFF: ...): the game, the VR session and the other pages carry on.
The page's drawing sits inside the hologram's existing fault guard (a fault hides only the hologram).

**live.txt (persisted).** `music_volume` 0-100 (`70%` works), `music_visualizer` original, bars,
fractal_bloom, tunnel, starburst or 0-4 (also `visualiser`, `fractal`, `bloom`), and `music_track` the song to
start from, 1 the first in A-Z order (0 none). The song is only picked at launch, never played by itself.

**Exports.** `SpidyMusicVolume(0-100)` (refused 3978), `SpidyMusicVisualizer(0-4)` (3979),
`SpidyMusicTrack(song + 1, 0 none)` (3980). `SpidyHudSettings` selector 3 gains bits 20 (the song), 27 (volume)
and 28 (visualizer); the new selector 7 packs the volume (bits 0-6), the visualizer (7-9) and song + 1 (10-25).

**Muting only the game's background music: not shipped (SM2TODO).** What was checked:
- The port has no hook into the game's audio engine (no Wwise RTPC, bus or music-volume address), and the
  SM2VR 0.0.27 binaries and the scans in this repo contain none either.
- The game's settings system (UISettingsMenuSystem, uisystemmenu.config, 123 numbered settings) is known,
  but which number is Music Volume has not been identified.
- PCGamingWiki: the game keeps its settings under `HKCU\Software\Insomniac Games\Marvel's Spider-Man 2`,
  and its Audio menu has separate Music, Speech, UI and SFX volumes. The registry value names are not
  verified, and the registry is most likely read only at launch, so it could not work live anyway.
- Muting the game's whole Windows audio session would also cut SFX and dialogue, so it is not used.
So the row is greyed with "Not built yet: for now set Music Volume to 0 in the game's Settings, Audio."
The next step: find the Music Volume setting's number through the settings system, verified in-game, then
lower it while the page's music plays and restore it on pause or stop.

**Previews.** `spidy_hud_preview <path.bmp> music`, saved as sm2-music-preview.png: the page with
each of the 5 visualizers (BARS with the list open and a row pointed at, FRACTAL BLOOM pointing at play, TUNNEL
paused), and the no-songs page scrolled to its end (visualizer picker, MUTE GAME MUSIC, the folder line). It is
fed a made-up signal, not a recording.

**Tests.** Core tests 455/455: FFT and bands (a 1 kHz tone lands in its band; silence, NaN), the triple
buffer under 200000 threaded writes, song list order and filtering, the MUSIC page's controls through the
Menu (list open/scroll/pick/close, every button, seek, a disabled button doing nothing, the picker wrapping),
help texts of 2 lines or fewer, and the visualizers' cost and sanity. Live-settings tests cover parsing,
export values, selector 3/7 decoding, persistence and the Music folder's README. Not tested here (no Windows
box with audio): the Media Foundation/WASAPI player itself; it is syntax-checked against the Windows headers.

## 29. Update 20: the wrist hologram opens, webs leave the wrist, realistic webs

**Opening the wrist hologram (the October 23 report: "nowhere does it bring up the menu").** The cause, from
that session's report: nothing was broken or switched off (no fault, `hud` on, the trigger read raw), but the
gesture almost never counted. Until Update 20 the pointing hand's *wrist* had to be within 11 cm of the
hologram hand's *wrist* point, and the trigger pull had to begin there. In the report the two wrists came
within 11 cm in 0.8% of the session (closest 7.3 cm); a trigger pulled anywhere along the forearm was always
out of reach. Now:
- **The gesture:** bring the right hand (its grip or wrist) within **15 cm** of anywhere along the left
  forearm, from the back of the hand to about 28 cm up the arm (the Quest handle's 30° tilt allowed for), and
  pull the **right trigger** past halfway. The light first touch of the trigger may begin out of reach as long
  as the pull past halfway happens in reach; from then the pull is the hologram's until it is let go. `hud_hand = right` mirrors it.
- **Fallback: F8** on the PC keyboard toggles the hologram (rising edge, read with GetAsyncKeyState; the game
  never binds F8). It works only in gameplay, as the gesture does; otherwise the console says why.
- **The log:** the mod exports `SpidyWristHudData` (192 bytes, seqlock: distance, nearest approach, the
  distance at each press, presses in/out of reach, refused because not in gameplay, toggles, F8 presses and
  refusals, and the music state). The launcher prints e.g. "Wrist hologram: pointing trigger pulled 9.3 cm
  from the hologram forearm (in reach)", keeps it in each sample (`wrist_hud`) and in the report.
- Risk: with the hands close a right-trigger web can now open the hologram. `hud = 0` turns the gesture off.

**Where webs leave.** The shooter was 10.5 cm along the grip's tilted +Y (above and behind the wrist), and
`webFromGrip` then pulled the start up to 15 cm on toward the grip centre, i.e. through the fist. Now
`webShooter` sits 5 cm back along the handle, 3 cm under it and 3.5 cm out on the palm side (the heel of the
palm / underside of the wrist), aimed as before along the controller (-Y), and `webFromGrip` moves the start
at most 2 cm on. Both hands are mirrored and tested. The fist's reach point (`fistAhead`) went from 15.5 to
10 cm so the punch's knuckles are where they were.

**The look.** MESSY is the new default (live.txt `web_style = messy`; 90s stays as an option). It is a fine
silvery-white strand: a translucent core all the way, five fine fibres twisted loosely round it (seen close),
three faint loose wisps near the hand, lit as silk (a sharp specular line and a faint sheen, both in
`web_shading.hpp` and the overlay's HLSL); no beads, knots or twinkling stars. Its tip and splat are soft and
translucent (fine fading threads) instead of the 90s opaque blobs. MODERN's twist stripe is fainter and it is
slightly translucent. SILVER and PEARL are brighter. Changes apply at once (no restart).

**Migration.** At the session start `live_settings.migrate_web_style` changes a live.txt line `web_style = 90s`
(the old default, which the hologram writes back) to `messy` once and adds the marker
`# Update 20: web_style default is now messy (migrated once; ...)`; with the marker there, a 90s chosen
again stays. Preview: `spidy_web_preview <prefix> hand` (a right hand: MESSY from the head, from the side,
90s from the side, the strand close up).

## 30. Update 21: what the October 23 logs show, wider HISTORY and SHARPNESS, the eye camera audit

**The October 23 (Update 20) session, from its report and console log.** The graphics settings changed in the
hologram did apply. The console shows each one taken (eye history 25, 40, 25, 15, 5%; sharpening 20, 40,
50, 30, 20%; eye anti-aliasing switched off and on; the web style ending on 90s). The report's GPU counters
agree: FXAA ran on 21,960 eye pairs and sharpening (CAS) on 59,804, ending at 20%. Nothing faulted, no
fail-safe went off, and the session never fell back to the flat screen. The report's `live_changes` was empty
only because it listed live.txt edits, not the hologram's; and the report was written after the game had
closed, so the eye history counters were missing. Both are fixed (below). The eye lenses were exactly as
asked (error 0). 29,441 eye renders were placed in the frame they were drawn, against 8 late. The frame rate
was 37.1 new pictures a second at 89.9 Hz.

Why the sliders did little: up to Update 20 they only reached the gentle part of each effect. CAS's top
(100%) is still mild, because its halo guard holds back exactly where facades are contrasty. History was
capped at 80% and held to the new pixel's 3 x 3 colour range, so it could never smear enough to show.

**Facade moiré (what it is and what it is not).** The window grids swim across the whole surface, not more
at the edges, and get worse with speed. That is aliasing of very fine texture detail against the pixel
grid. It changes with any rotation and is not filtered over time, because each eye is rendered without the
game's TAA/DLAA (the game's DLAA/DLSS skips its offscreen views; the eyes' own temporal pass is off,
`settings+0x424`). The low frame rate, each picture shown about 2.4 times, makes it worse. It is not a stale
camera matrix or a wrong inverse projection; see the audit in the report. Shadow strobing in grooves is the
same thing on the shadow maps (per-eye cascades, no TAA). Flat windows come from RT Interiors, which the
session forces off; matte glass from RT Reflections being off, and the eyes' SSR targets are not ported.

**HISTORY 0-95% (was 0-80).** Up to 50% it works as before. From 50% the colour range the old picture is held
to widens too (`eye_history::historyLoosen`, up to half the range on each side at 95%), so 80-95% is plainly
heavy: much calmer facades and visible trails behind motion. A cut (a turn of more than 15° between two
pairs) still starts the history again. The hologram steps in 5s. live.txt `history = 0-95`.

**SHARPNESS 0-200% (was 0-100).** 1-100% is CAS exactly as before. 101-200% keeps CAS's strongest lobe and
lifts its halo guard step by step (`eye_sharpen::boost`), so 200% sharpens every pixel fully: clearly
visible, with halos and more shimmer. The denominator never drops below 0.2, so it cannot blow up. The
hologram steps in 10s. live.txt `sharpen = 0-200`. The hologram's settings read-back (SpidyHudSettings
selector 4) now carries the sharpness in 8 bits and the reticles in bit 8.

Defaults are unchanged (both off until set; FXAA on). Both apply live, with no restart.

**Logs.** Every graphics change is now listed in the report's `live_changes` with its `source` (`live.txt` or
`hologram`). At the end of the session the console prints "Graphics settings: history changed 5 time(s)
(hologram): 25 -> 40 -> ...; 5 at the end." for aa, history, sharpen, web_style, reach and monitor. It does
this whichever way the session ended, including when the game was closed first. The eye history counters
are sampled each second (`eye_history` in the samples) and kept, so a report written after the game closed
still has them.

**Eye camera audit (FrameData version 5, 368 bytes).** This tests the other analysis's hypotheses 1 and 2
directly, with no new game address. Each eye job copy compares the rendered camera position (view +0x30)
with the pose given SetPose that frame, and the previous-frame camera (view +0x560, which the engine's
motion and history matrices come from) with the eye's own pose of the frame before, to 1 mm. It also
measures the rendered eyes' distance apart. Report: `eye_jobs.eye_camera` (and in each sample); console:
"Eye cameras: N of M eye renders from the pose given that frame, L a frame late, E from elsewhere ...;
previous-frame camera the eye's own in P of Q; eyes 63.8 mm apart (63.1-64.4)."

Still unknown (no verified SM2 address, so nothing is changed there): where GlobalWorldCBuffer is and its
layout (Remastered's m_ActiveViewCtxPos +330 is unverified for SM2), the per-eye constant buffer offsets,
and the field offsets of the inverse / previous view-projection matrices inside the view.

## 31. Update 22: a shareable package (no Python to install)

A friend running a checkout got `[ERROR] Failed to launch 'C:\Python310\python.exe' (0x80070003)`: the
starter took the first `python.exe` on PATH, a shim left pointing at a removed Python 3.10.

- **`tools\package.ps1`** now makes `dist\SM2-VR-<PORT-VERSION>-win64.zip` (`SM2-VR-Update-22-win64.zip`).
  It holds the game modules, Spidy Launcher.exe (if built), the Python tools a session imports, the embeddable
  Python 3.12.10 (hash-pinned; `..\tools` added to `python312._pth`), `Launch Spidy VR.cmd` and
  `Launch Spidy Game VR.cmd`, `PORT-VERSION`, `SHARING.md` as `README.txt`, an empty `Music\` (with a note),
  `LICENSE.txt`, `THIRD_PARTY_NOTICES.md` and `docs\licenses\`. It leaves out live.txt, next-launch.txt,
  reports, songs, sources, `.git` and build intermediates (and fails if any slipped in). The zip is written
  with `ZipFile` (forward slashes, the folder at the top), not `Compress-Archive`.
- **Contents from the code.** `tools\package_manifest.py list` follows `run_game_vr` and `xr_runtime`'s
  imports with `ast` (imports inside functions too) and names the binaries the tools and `launch-game-vr.ps1`
  ask for. Today: 21 tools (the same list as the old hand-written one), 10 modules plus the haptics test.
- **Self-test that fails the package.** The packaged Python runs `package_manifest.py check` on the packaged
  tools (every tool present, every standard-library import resolvable, `run_game_vr` and `xr_runtime`
  import), then `run_game_vr.py --help`, all from a staging path with spaces (`build\package\space test\`).
- **Python choice.** The package's own `python\python.exe` is always first. Outside a package,
  `launch-game-vr.ps1` and Spidy Launcher.exe (`findPython`) run `python -c "import sys;
  print(sys.version_info[:2])"` on each candidate (PATH entries, the py launcher's, `%LOCALAPPDATA%\Programs\Python`)
  and skip any that fail or are older than 3.11. If none works they say plainly: use the shareable package, or
  install Python 3.12 from python.org with "Add python.exe to PATH" ticked.
- **Runs from the package.** `launch-game-vr.ps1` skipped its "build older than sources" check only by
  throwing when `src\` was missing; the check now runs in a checkout only, and a missing module in the
  package says "unzip the whole package again" instead of "build first".
- **C++ runtime check.** The modules use Microsoft's C++ runtime (`/MD`), which must be 14.40 or newer.
  `launch-game-vr.ps1` now checks `msvcp140.dll`, `vcruntime140.dll` and `vcruntime140_1.dll` in System32
  and stops with the vc_redist.x64.exe link instead of letting the game fail to load them.
- **Windows N.** Media Foundation (`mfplat.dll`, `mfreadwrite.dll`, used only by the music player) is
  delay-loaded, and the player checks that both exist first. A Windows N/KN PC without the Media Feature
  Pack still loads the stereo module and plays in VR; only the music says it is off.
- **SHARING.md** is the friend's README: requirements, unzip and run, Music, opening the hologram, and which
  report files to send back.

Not tested on Windows yet: the whole package run, the new Python choice, the C++ runtime check, and the
delay-loaded Media Foundation (Linux builds the launcher's text helpers and the tests only).

## 32. Update 23: what the October 23 (second) logs show, supersample OFF, webs from the wrist, history while turning

The session ran **Update 21** (its report's `port_version`), not 22, with `--overlay-webs --render-scale 100
--upscale 67 --mip-bias 0 --anisotropy 16 --eye-aa fxaa --supersample 100`. Eye supersample was 100 in both
sessions, so it was not what held the frame rate down. New eye pairs: 35.4/s on average (slowest tenth 29.4)
on a 90 Hz headset. With the wrist hologram open, `hand_overlay` took 3.1-3.9 ms and the rate was 29-38/s; with it
closed, 0.4 ms and 44-50/s in the same streets. The game's own settings were heavy (shadows, LOD and crowds Ultra,
SSR High, hair Very High). All 23 022 eye renders used that frame's pose (0 late, 61.3 mm apart): no camera fault.
The webs were the mod's overlay webs (`gameWeb` false in all 64 snapshots).

- **Supersample OFF by default.** The GRAPHICS page shows 100% as **OFF**; its line says "OFF is fastest; higher
  calms shimmer but costs much frame rate (needs a restart)." `live_settings.migrate_next_launch` resets an older
  `next-launch.txt` above 100 back to 100 once (and says so); files written from now on carry
  `SUPERSAMPLE_MIGRATED`, so a value chosen again stays. It is still labelled NEXT LAUNCH - RESTART REQUIRED.
- **Webs from the wrist.** Update 20's spot (`webShooter`, still the aim line) was 5.5 cm out from the wrist
  joint body_ik draws, past the palm's skin and 3 cm toward the little finger: under the drawn hand, in the air.
  Webs are now drawn (overlay webs, the native rope's wrist and the release animation) from `webStart`: the
  hero rig's webStart joint, 1.5 cm toward the elbow from that wrist and 3 cm out on the palm's side, both hands
  alike, with no shift toward the fist. live.txt `web_start = palm` (or 1) draws from Update 20's spot again; it
  is applied live (export `SpidyWebStart`, 3990 refused) and kept for the next session. Aim, attach and the
  hologram pointer are unchanged.
- **History while turning.** The history only turns the old picture; while the head turns, shadows and edges
  that shift against the buildings were held back by the loosened clamp (HISTORY above 50%) and dragged.
  `eye_history::turnedWeight/turnedLoosen`: the loosening falls to none at a cut's turn (15 degrees between two
  pairs), the weight to 40% of itself; a still head keeps both.
- **Mip bias default 0.5** (was 0) in `launch-game-vr.ps1` and `run_game_vr.py`: less sparkle and flicker on roofs
  and far textures, a little softer. Only when the launcher starts the game; `-MipBias 0` is the old look.
  The mip bias is read once early in the session too, so a report written when the game closed first has it.

Not changed, and why: the facade moire and roof-edge flicker are geometry finer than an eye pixel, which the game's
own DLAA/TAA would settle with a moving sub-pixel jitter and motion vectors per eye. That needs the per-eye TAA
history, jitter and motion-vector setup in SM2 (SM2TODO: the SM2 counterparts of SpidyVR's SpidyEyeTemporal
offsets; not found, so not guessed) or a mod-side TAA with its own jitter and the game's depth and motion vectors
(a large job). The hologram's frame cost is its draw; it was not cached (a cached copy would lag the panel's
turn by a frame); close it while playing.

## 33. Update 24: why far things flicker (the October 23 third logs), EYE TEST, upscale 100

The session ran **Update 23** (`port_version`), mip bias 0.5 on 354 samplers, `web_start` default (wrist; no
live.txt line). New eye pairs averaged 42.4/s (slowest tenth 36.3) on a 89.9 Hz headset: each pair is shown about
2.1 headset frames, so Virtual Desktop's reprojection (SSW) fills in. Left and right copies matched (16 563 each)
and all 33 126 eye cameras used that frame's pose, 61.4 mm apart: no alternating eyes, no late pose.

What makes the far distance flicker, most likely first:

1. **No game TAA in the eye views.** The game's DLAA/DLSS/TAA (and its AA and quality menu) only run in the
   1440x810 flat monitor view; the eyes get FXAA at most. Roof edges, windows and facades finer than an eye pixel
   alias into moire and crawl. This is why lowering the game's settings changed nothing. SM2TODO (per-eye TAA
   jitter/history offsets in SM2; not guessed).
2. **Eye LOD and texture distances.** SpidyVR matches the eyes' detail distances to the flat camera
   (`matchDistances`), gated on byte checks at Remastered addresses that do not match SM2
   (`eye_detail_frames 0`, `eye_distance_scale 0`). The eyes keep their own 110x102 degree lens distances, so
   detail and texture sharpness drop at about half the flat game's distance and pop in and out. SM2TODO.
   The game's Level of Detail at Very High pushes it out.
3. **Each eye fits its own shadow cascades** every frame (sharing crashed SM2VR 0.0.4), so far shadow edges
   land differently in the two eyes and swim while turning. No SM2 flag known: SM2TODO.
4. **Sharpening 190% (strong mode)** kept from the hologram also sharpens that flicker and the video encoder's
   noise. Live: SHARPNESS. The launcher now says so when the session starts.
5. **Virtual Desktop** at about 42 of 90 Hz: SSW/reprojection smears ("drags") shadows and edges while the head
   turns; fine far detail is also where the video codec loses most. Headset-side settings.
6. Depth precision: the eyes use the game's own near/far planes (the flat camera's): not a cause on its own.

Ruled out: **upscale 67** (no SM2 eye upscaler: SpidyEyeTemporal sets no temporal mode, the eyes rendered at
2688x2880, `eyes.upscale` 100), alternating eyes, and depth reprojection: `xr_session.cpp` enables only
`XR_KHR_D3D12_enable`, submits one opaque `XrCompositionLayerProjection` with no depth
(`XR_KHR_composition_layer_depth` not used) and no foveation extension.

Changes:

- **EYE TEST** on the GRAPHICS page (live, ON/OFF, never kept): ON shows both eyes the left eye's picture
  (flat). If far flicker stops, it was the two eyes disagreeing (shadows/LOD); if it stays, it is aliasing or
  the stream. Only the copy into the headset's swapchain changes; the game renders as before; overlays are drawn
  per eye so they may look doubled. live.txt `eye_debug = same`/`off` does the same (export `SpidyEyeDebug`,
  3991 refused; not persisted; START notes remind if it is left in live.txt). The page now scrolls to NEXT LAUNCH.
- **Upscale default 100** (`-Upscale`, `--upscale`, `defaultEyeUpscale`): 67 had no effect in SM2.
- **Session-start notes** (`live_settings.start_notes`): strong sharpening kept, an eye test left on.

What can be set where:

| Setting | When it applies |
| --- | --- |
| EYE TEST, SHARPNESS, HISTORY BLEND, ANTI-ALIASING, BUILDING REACH | live (hologram GRAPHICS, live.txt) |
| EYE SUPERSAMPLE | NEXT LAUNCH, labelled RESTART REQUIRED |
| Render scale (`-RenderScale`), mip bias (`-MipBias`, set when the game makes its samplers) | restart (launcher) |
| Near plane | not ours: the game's own camera near/far |
| Shadow cascades shared | no SM2 flag (SM2TODO); sharing crashed SM2VR |
| Depth layer | none submitted, so nothing to toggle |
| Upscale | no effect in SM2 (no eye upscaler) |
| VD foveation, SSW, sharpening, codec, bitrate, refresh rate | Virtual Desktop in the headset only |

### Session FPS: HIGH, LOW, AVG and RESET

The VR SETTINGS page shows the session's **HIGH / LOW / AVG** beside the FPS monitor, with **RESET** (live).
They count what the FPS monitor counts (headset frames showing an eye pair not shown before), in whole
1-second windows one after another, starting 10 s after the first new pair (loading and the first streaming
hitches left out), or at once after RESET. **LOW** is the slowest whole second; a second with no new pair for
over 0.5 s (a loading screen, a pause) is left out, so one long load is never the LOW. HIGH is the fastest
second, AVG all tracked pairs over all tracked seconds. run_game_vr.py reads them every 5 s
(`SpidyHudSettings` selectors 8 and 9, `live_settings.session_fps`) into the report (`session_fps`) and prints
`Session FPS ... HIGH, LOW, AVG over N s` when the session ends. A/B test: change one thing, RESET, play the
same route a minute, read the three numbers.

### Unzip-and-play package

- **No Visual C++ Redistributable.** `CMAKE_MSVC_RUNTIME_LIBRARY` is `MultiThreaded` (/MT) for every module and
  program; the OpenXR loader (its CMake forces /MD) and MinHook are set after `add_subdirectory`. Inside the
  game an app-local msvcp140.dll would not help (Windows binds to the one the game already holds), so nothing
  is bundled. bHaptics' prebuilt `bhaptics_library.dll` imports VCRUNTIME140, which the game loads itself; if it
  ever cannot load, haptics stay off and VR goes on. `spidy_haptics_test.exe` run on its own may still need it.
  `tools/player_check.py runtime DIR` and the launcher (`include/spidy/pe_imports.hpp`) read the modules' PE
  imports: an older /MD build still gets the 14.40 check and the download link, an /MT build "built in".
- **Defaults with no flags** (`launch-game-vr.ps1`, so `Launch Spidy VR.cmd` = the command line): overlay webs on
  (`-GameWebs` for the game's own; `-OverlayWebs` still accepted), `-RenderScale 100`, supersample off (a
  hologram NEXT LAUNCH choice still applies). Spidy Launcher.exe passes `--overlay-webs` too.
- **Before a session** (`player_check.before_session`, in run_game_vr.py before anything is written): the folder
  must be writable (else a plain stop); Program Files, a OneDrive-synced folder and a path too long for
  `reports\stereo-modules\<sha>\...` are warned about; the game is looked for in every Steam library
  (`libraryfolders.vdf`, `appmanifest_2651280.acf` installdir) then Epic. Not found is a note (Steam still
  starts it by app id); a readable other version stops with "Your Spider-Man 2 is version X; this Spidy supports
  2.810.0.0"; Windows' active OpenXR runtime is named, or "no active runtime" / "No OpenXR runtime is installed".
- **Unblock.** launch-game-vr.ps1 runs Unblock-File on the files in its own folder, tools, python and
  build\windows-ninja that carry the downloaded-from-internet mark (never outside the folder).
- **Layout.** The package's top holds only `Launch Spidy VR.cmd` to start (README.txt says "Unzip, start your VR
  app, double-click Launch Spidy VR.cmd"); Spidy Launcher.exe is `tools\Spidy Launcher.exe` (it finds the
  package by looking up; its self-update is off there, which keeps upstream SpidyVR releases out of an SM2
  folder). The checkout's other .cmd files are not packaged.
- **package.ps1 self-test** also runs `player_check.py runtime` (no module imports the Redistributable) and
  `player_check.py folder` on the staged folder, checks the single starter, the README's first line, that
  launch-game-vr.ps1 parses and has Unblock-File, the overlay default, RenderScale 100 and Supersample 100.

### Web fibres: every style a twisted fibrous strand

From the player's reference photo (a fine twisted silvery strand with loose
wisps and frayed loops, not a smooth tube or a ribbon), all five WEB STYLEs
are now laid as a twisted strand. Swinging is unchanged; only the look moved.

- **Core.** Near the eye the rope is three real 3D plies twisting round its
  line inside its own radius, so its thickness (and `web_width`) is the same
  as before, all along. As the rope narrows on screen the plies close up into
  one tube (none under about 6 pixels across), and further off the tube
  carries the twist in its shading alone, which fades out in turn before it
  could shimmer.
- **Fibre shading.** Each ply shows fine fibre lines and a hair highlight along
  its twist (web_shading.hpp `fibres()`, mirrored in the HLSL); they fade out
  by their own size on screen, so a far web is the plain lit strand of before.
- **Loose fibres.** A few dozen seeded loops and curling wisps stand off the
  rope near the hand (denser close up), drawn as half-pixel-floored strips at
  their true width's share of opacity; they fade out with distance and are
  never drawn past where the plies close up.
- **Per style.** MESSY: silvery and the most frayed. MODERN: a tight even
  twist, few loose ends. 90s: the cream cartoon rope keeps its inked coils,
  now with inked ply seams spiralling round it up close and a few cream fibres.
  SYMBIOTE: black, slick plies alive between hand and anchor, with tapering
  tendrils. ENERGY: three glowing plasma plies with plasma lines and small arcs
  crawling round it, the glow band kept.
- **Cost.** All live (no restart): a web stays inside the 24000-vertex budget
  (a style that would not is drawn as 90s, as since Update 18; never lost).
  90s itself: its loose fibres are cut first to stay within it, and its
  tighter twist steps are capped (32 more at most); the draw buffer grows if
  ever needed, so no web can crash it.
  Preview: `spidy_web_preview <prefix> fibres` (all five, up close, about a
  metre out and 4 m away at headset pixels).

## 34. Update 24.1: Update 24 went to flat screen at gameplay (the fix)

The October 23 07:23 log said, as gameplay began:

    Gameplay found; waiting for the first eye images
    Body: on, 234 joints.
    Compile vertex shader HRESULT=2147500037

2147500037 is E_FAIL from D3DCompile. Update 24's web shader named a variable
`line`, a word HLSL keeps for geometry-shader inputs, so the overlay shader
(hands, webs, wrist hologram) no longer compiled. D3DCompile reads the whole
source for each entry point, so the vertex shader failed first. It is compiled
at the first drawing; its exception reached the XR thread's last catch and VR
ended in flat screen. The /MT runtime change in Update 24 was not involved.

- The variable is `fibreLine` now (and Update 11's `half`, legal but a type
  name, is `ropeHalf`). All nine built-in shaders compile with DXC.
- A shader that does not compile no longer ends VR: the overlay is drawn in
  its plain vertex colours and the console says so, with the compiler's own
  message. Any other drawing failure turns Spidy's drawings off for the
  session ("VR continues; Spidy's drawings are off this session: ...").
- The blit shader's errors carry the compiler's message too.
- Whenever VR does end in flat screen the console says so in one line:
  `VR STOPPED - the game is back on its flat screen. Reason: ...`. A part that
  then stops uncleanly adds to that reason instead of hiding it, and a session
  the VR runtime closed (headset, Virtual Desktop, SteamVR) says so.
- `tests/shader_safety_tests.py` keeps HLSL reserved words out of every
  shader, and compiles them all with DXC when it is installed.

## 35. Update 25: shadows that slide up and down buildings as you look around

The 24.1 report (October 23 07:43): every eye render used that frame's pose
(eye_camera as_asked 36528 per eye, miss 0; "73056/73056 eye renders from
that frame's pose"), the eyes were 61.4 mm apart, exposure was shared
(73056). The game ran windowed at 1440x810 (16:9) with DLAA, Shadow Quality
Medium, RT shadows and AO off. The game's own view was moved off the head
pose's final offset at its early submit in 18309 frames, by up to 1.105 m
(active_view_lag_frames).

The flat clip the player took is the game's own window (1440x810, the game
view on the head, mono): this port never sets createViews flag 32, so the
window shows the game's own view and its own shadow fit, never a mirror of
an eye. Its shadows are steady; the eyes' slide. So the fault is in how the
EYES' shadows are made, not in the shadows themselves.

The eyes are offscreen views. Each eye fits its own key-light cascades to its
own lens (SpidyShadowData), but the frame's lighting constants (SM2's
GlobalWorldCBuffer: the active view's position and the key light's shadow
set-up) are made once a frame for the game's own view, on the head with the
lens covering both eyes. Shadow lookups that mix the eye's cascades with the
game view's set-up land in the wrong place by an amount that depends on how
the two frustums differ, which changes with pitch: the slide is opposite to
where you look, and a moving shadow turns a little as the head turns.

Ranked causes:
1. Each eye's own cascade fit, read with the game view's frame-wide shadow
   set-up (the most likely: pitch-dependent, steady on the monitor).
2. The frame-wide constants made at the early submit (head pose without its
   final offset, up to 1.1 m apart), or a frame old for the eyes.
3. The camera-relative view position (m_ActiveViewCtxPos) being the head's,
   not each eye's: a fixed 32 mm shift, not pitch-dependent.
   Fixing 2 and 3 needs GlobalWorldCBuffer's address: SM2TODO.
Ruled out: the eye cameras (perfect), AO (off), SSR (the eyes have none),
exposure (shared).

Correction (Update 36, docs/main-view-gates-audit.md #3): cause 1's premise is
wrong. GlobalWorldCBuffer is built per render job, not once a frame: each job's
context is copied to 0xc2c64a0 (ViewContextInit 0x2c81890, memcpy at 0x2c8191c)
and the CB is filled from it (0x2c82010 -> 0x2c6b6d0 -> 0x2c67f40), with the
job's own view (+0x650), cascades and view settings (+0x658) from copyJob
0x2d03240. Each eye's CB is its own. What does differ per view is in the view
settings 0x2c27ec0 builds: the eyes have no bloom (+0xd8, zeroed for any view
that is not active at 0x2c29102), which Update 36's EYE BLOOM gives back.

What changed:
- EYE TEST on GRAPHICS is now < > through OFF / SAME PICTURE / HEAD SHADOWS
  (live.txt `eye_debug = off | same | shadows`), live and never kept.
  HEAD SHADOWS keeps 3D (each eye its own picture) but fits BOTH eyes' shadow
  cascades to the game view's own head lens and pose, through the cascade
  fit hook (2dd8d20, SM2VR's bytes), with nothing shared between the eyes.
- It is refused where the hook is not in the game, and turns itself off (the
  hologram back to OFF, one line: "VR continues; EYE TEST HEAD SHADOWS is
  off: the shadow fit never ran") if the fit does not run for 240 eye renders.
- SpidyShadowData version 2 (784 bytes) adds HEAD SHADOWS' counts and how far
  each eye's nearest cascade sits from the game view's own each frame; the
  report has `eye_shadows`, and the console an "Eye shadows:" line at the end.
- The web visuals are unchanged.

What to look for: stand still, look down at a building's shadow, then up.
With HEAD SHADOWS the shadow should stay put (cause 1). If it still slides,
cause 2 or 3 it is, and that needs GlobalWorldCBuffer's address (SM2TODO).

## 36. Update 26: SAME PICTURE's right-eye warp, HEAD SHADOWS removed, the package's own Python only

The Update 25 report: eye_lens[0] = [-1.37638, 0.8391, -0.96569, 1.42815] and
eye_lens[1] = [-0.8391, 1.37638, -0.96569, 1.42815], asked = used (eye_lens_error 0.0,
4453 frames), both eye snapshots 2688x2880 with full-eye imageRects and no depth layer.

- Right-eye warp: with EYE TEST SAME PICTURE the right image is a copy of the left
  render (eyeDebugSource(Same, 1) = 0), but the right projection layer still said
  the right eye's mirror-image fov and pose: about 0.54 of horizontal tangent off,
  so the picture slid and stretched sideways in that eye only. The copied pair now
  records the mode it was made with (native_gpu::pairEyeDebug), and the layer, the
  grade and the overlays use the fov and pose of the eye the picture was made
  from (made(eye)). SAME PICTURE now looks flat but not warped. OFF is unchanged:
  its render and layer fovs come from the same frame.eyes.
- HEAD SHADOWS: the log says "VR continues; EYE TEST HEAD SHADOWS is off: the
  shadow fit never ran", then "[App] Quit requested from pause menu" and a clean
  shutdown: no crash, no hang, the auto-off worked. SM2 never fits a cascade
  inside the eye submit, so the switch could never work: it is removed (wrist,
  live settings, exports). eye_debug = shadows is refused with a line.
- Python: the starter and the launcher tried the package's python\python.exe and,
  if it was missing or would not start, went on to the PC's Pythons, so a friend
  saw "older than 3.11" for a Python Spidy should never have touched. A package (a
  python folder, or no src) now runs only its own 3.12.10; if that is missing or
  broken it says so and that installing Python will not help. -Python is ignored in
  a package. package.ps1 checks the staged Python is exactly 3.12.10.
- Exit reports keep the last eye_shadows sample.

## 37. Update 27: ADDRESS FINDER (read-only candidates for missing shadow / AA data)

**Update 27.4:** core CTest — the Update 27 ADDRESS FINDER test used an identity pose, which 27.3's trivial reject correctly drops (`pose/jitter/proj should match`). Not MSVC-specific: it fails on Linux at 27.3 too. The test camera is now a yaw+translation pose. The combined test is split into 7 named tests: matchers, trivial reject, K-frame confirm (changing values over 3 frames), signatures, LEA xrefs + ASCII, pdata, JSON escape + jitter track. `spidy_tests` reprints FAIL lines at the end. On failure, `build-native.cmd` re-runs the failed CTest, and `build.ps1` prints a red FAILED TESTS block (from `Testing\Temporary\LastTest.log`) as its last output.

**Update 27.3:** ADDRESS FINDER quality gate — reject trivial zeros/identity/±1/NaN/denormals; confirm a hit only after K=3 frames with changing values; dedupe by call site + CB offset (not GPU VA); HUD `DONE n` = confirmed count while `rawHits` is the capped store (cap never ends the run); `frameCount` ticks once per stock `noteView`, not per barrier (27.2 ended in ~1 s at maxFrames because barriers fired thousands of times); early finish only when lighting, TAA/jitter and LOD each have a strong confirmed hit, else the full ~60 s window. Fresh start always clears prior DONE. `tests/address_finder_tests.py` QualityGate covers cap / trivial / confirm.

**Update 27.2:** ADDRESS FINDER must never stall a frame — heavy string/xref/signature work and report finalise run on a low-priority background thread; per-frame GPU/CB sampling is hard-capped (~0.5 ms / 8 ops) with automatic skip backoff; reports refresh every ~10 s and on finish/fail/session end into `reports/address-finder-*.json|.txt` plus `address-finder-latest.*` (look in `C:\sm2-port\reports`, not `tools\`). `tests/address_finder_tests.py` FrameBudget covers the budget helpers.

**Update 27.1:** MSVC `/EHsc` C2712 — every `__try`/`__except` in `native_address_finder.cpp` lives in a small POD-only helper (`safeRead`, `sehVtable`, `sehMap`, `sehUnmap`, `sehGpuVa`, `sehGetResourceDesc`). `tests/seh_safety_tests.py` flags the old pattern.

Spider-Man 2 still has no known GlobalWorldCBuffer builder/uploader or TAA/DLAA
jitter/history/mvec addresses (SM2TODO). Update 27 adds a press-only **ADDRESS FINDER**
on the hologram GRAPHICS page (and `live.txt address_finder = on|off`) that runs a
~60 second read-only scan while you play normally.

- Scans: D3D12 CB/root/barrier callouts (no second barrier hook), Streamline/NGX/XeSS/FSR
  export detours, static string + LEA xrefs, RenderAlloc + active-view LOD float match.
- Output: `reports/address-finder-<time>.json` and `.txt` — **candidates only**, never
  applied as live addresses. Console: `ADDRESS FINDER done: N candidates, report saved to …`.
- Safety: SEH, time (~60 s) and frame caps; any fault disables only this path and logs one
  OutputDebugStringA line. Never falls back to flat screen and never crashes the session.
- HUD subtext: one sentence ≤80 chars. Status shows START / ON nn S / DONE nn / OFF (FAILED).
- Exports: `SpidyAddressFinder`, `SpidyAddressFinderData` (SAFD), `SpidyAddressFinderReport`.
- Not persisted. Not a setting bit in the 32-bit mask (press like FPS RESET).

Player steps: open the hologram → GRAPHICS → ADDRESS FINDER → play ~1 minute → send the new
`address-finder-*.json` / `.txt` with the usual `game-vr-*` report pair. If the finder finds
nothing useful, x64dbg / Cheat Engine on the same targets remains the fallback (still
candidates only until a later update wires a verified address).


## 38. Update 28: what the two 27.4 ADDRESS FINDER runs found, a wider gate, and EYE LOD

Both runs on Oct 23 (`address-finder-partial-20261023-132247.json` and the finished
`address-finder-latest.json`) ended `DONE 1`. All RVAs agree between the runs and with the
27.2 list. These are candidates only: none of them is used as a live address.

- **Confirmed (1):** `sl_constants` call site rva 0x3185cd7 (function 0x3185b60), the
  Streamline `slSetConstants` caller.
- **Why only 1:**
  - The 256-row raw store filled up with one-frame non-value hits, so the cb+0x2e00 jitter
    track (hits 3 → 4) never got a `taa_jitter` row.
  - The dedupe re-keyed on the function once it was known, which gave nine rows for one
    history call (0x2cc66fe).
  - All 127 `cb_match` rows are jitter matches. They are ~25% off a 4e-4 jitter, so they got
    in only because of 27's absolute 2e-4 tolerance (noise). 66 of them share one frozen
    camera jitter.
  - No matrix, position or projection matched. The CB is built from an older camera, and
    camera-relative or combined view-projection forms were never tried.
  - The LOD scale (view+0x1b24 = 0.4667, the eye lens's) cannot change, so the
    "changing values" rule could never confirm it.
- **28's finder:**
  - Jitter uses a relative 3% tolerance, also scaled x2 or x0.5 and/or with y flipped.
  - New matrix forms: camera-relative view (R^T), view·projection in rows or columns, and its
    inverse.
  - Each buffer is matched against the newest camera and the three cameras before it
    (`lag=`).
  - Near misses (up to 4× tolerance) are logged as `cb_near`.
  - Non-value hits stop at 3/4 of the store, and the store evicts one-frame rows first.
  - Dedupe keeps one row per call site.
  - A stable value confirms when it is seen at the same offset on K separate frames.
  - A jitter track confirms only if its value changed.
  - The report and `.txt` list **CONFIRMED**, then **ALMOST** (unconfirmed but seen with a
    value on 2+ frames, or a near miss), then the rest. The HUD shows `DONE n +m ALMOST` and
    the console says `n confirmed, m almost`.
- **EYE LOD** (GRAPHICS page; `live.txt eye_lod = on|off`):
  - A live press. Default OFF, never kept, takes effect at once (no restart).
  - Subtext: "Draws model detail and textures as far out as the flat game does (test)."
  - Each frame it writes each eye's view+0x1b24 as the eye lens's scale × `eyeDetail`
    (the 68-degree lens's distances, at most 3×). It writes only while the value there still
    equals the eye lens's own distance scale.
  - If the value stops matching for ~2 s, or a read or write fails, EYE LOD turns itself off
    for the session (`OFF (FAILED)`), gives back the original value and logs one
    `EYE LOD: disabled` line.
  - The Remastered multiplier's SM2 place (+1cc8 there) stays SM2TODO.
  - Telemetry: `eye_detail_frames` in the game-vr report counts frames it applied.
- **Per-eye DLAA:** not built.
  - What exists: the Streamline call sites are stable and jitter is decoded.
  - What is missing: no camera matrices, per-eye depth/motion-vector tags, viewports or
    history are known.
  - Next step: an ADDRESS FINDER run on 28 for the matrices.


## 38.1 Update 28.1: why EYE LOD failed on Oct 23, and EYE LOD by range

**What the Oct 23 2026 session (game-vr-20261023-135350, Quest 3, render 100%) showed:**
- The hologram said `EYE LOD OFF (FAILED)`. Neither log had an `EYE LOD` line, and the report had
  `eye_detail_frames 0`: no frame was ever written.
- The eye lens was about 94 by 99 degrees (`eye_lens` tangents −1.376…0.839 by −0.966…1.428).
  Its own distance scale is 0.6745 / tan(47°) ≈ **0.629**.
- The 0.4667 that the 27.4 ADDRESS FINDER found at +1b24 was the **active view's**. Its lens covers
  both eyes (`active_view_fov_deg` 110.6 × 101.7): 0.6745 / tan(55.3°) = 0.4667.
- Update 28 wrote only while each eye's +1b24 equalled that eye's own scale (0.629, relative 1e-3).
  It never did, so every frame was a mismatch, and after 120 frames (~1.3 s) EYE LOD gave up.
- Its one line went to `OutputDebugStringA` only, which no console or log shows. That was a bug of
  its own.
- EYE LOD never depended on ADDRESS FINDER (it uses the constant 0x1b24, not `lod_offset`).
- A failed SEH read would also give 0 frames. It is unlikely on a pool view that big, and the new
  record now tells the two apart.

**Update 28.1:**
- **Read first, judged by range** (`native_view::eyeLodStep`):
  - A value is in range if it is finite and within [0.05, 4].
  - It must hold still for 30 frames before the first write.
  - What is written is the engine's value × the eye lens's `eyeDetail`, or the flat game camera's
    own scale if that is further. It is never below the engine's value and at most 3× it.
  - EYE LOD holds what it wrote, writes again when the engine puts its own value back, and never
    compounds.
- **Fails on:**
  - 120 frames out of range;
  - ~10 s without a steady value;
  - a failed SEH read or write.
- **When it fails:** both eyes get their own values back, EYE LOD stays off for the session, and
  nothing else changes.
- **The reason is always recorded once:**
  - in `SpidyEyeLodData`, which the launcher prints as one `EYE LOD: off for this session - <reason>`
    line;
  - in the report: `eye_lod_status`, `eye_lod_reason` and `eye_lod` (engine value, written value,
    eye-lens and flat scales, frame counts);
  - in the debugger log;
  - on the hologram, whose subtext under `OFF (FAILED)` reads `Failed: <reason>.`
- **Appearance telemetry:** `eye_distance_scale` is now the engine's +1b24 value and `eye_detail` is
  the factor applied.
- **`tools\collect-reports.ps1`:** zips the newest game-vr json, its logs and
  `address-finder-latest.*` into `C:\sm2-port\reports\send-me.zip` and prints the path.
- **Desktop:** a OneDrive desktop is not the Desktop folder under the user profile. Find the
  real one with `[Environment]::GetFolderPath('Desktop')`.

## 39. Update 29: the Oct 23 28.1 run, and which camera the finder's hits came from

Diagnostics only. Gameplay and every setting are unchanged.

- **What the 28.1 run showed:**
  - EYE LOD ran (+0x1b24 0.6089 raised to 0.968) at about 49–78 s and 108–150 s. Frame rate and
    flicker were the same with it on and off, so +0x1b24 is not the LOD that flickers.
  - The slow stretch (12–26 fps) was the ADDRESS FINDER running (116–142 s). The peak (59 fps)
    came after it. The ~6 fps at the end was the stream stalling while the game closed.
  - SAME PICTURE gives both eyes one picture, so there is no depth and the world looks the wrong
    size. That is expected; it is a diagnostic only.
  - The finder matched the flat game camera's position exactly (err 0) at many constant-buffer
    offsets. That camera sits 4.7 m from the head and does not pitch with it, which fits the
    shadow on near walls that slides with head pitch. This is not yet proven.
- **Fixes:**
  - The report written when the game closes now has `eye_lod_status`, `eye_lod_reason` and
    `eye_lod`. Every report also has `eye_lod_timeline`: (seconds, status) at each change.
  - The console prints `EYE LOD: off - ...` when it is turned off after having run.
- **ADDRESS FINDER (still read-only, default OFF):**
  - It now also tries the head view and both eye cameras (`noteView` 2 and 3), not just the
    flat camera and its last three frames.
  - It tries identical cameras only once. Each hit lists every camera it matched
    (`cams=flat+flat-1+head`); `lag` is the newest flat frame. In 28.1, `lag=3` was an artifact.
  - Tags: `FLAT-ONLY` (the game's camera while the head was at least 0.25 m away), `HEAD-ONLY`,
    `EYE-ONLY`.
  - The txt has a `--- WHICH CAMERA ---` summary with counts and the functions involved. All
    addresses remain candidates.

## 40. Update 30: which pass the camera data feeds (BIND TRACE, SHADOW PASS CANDIDATES)

Diagnostics only. Gameplay and every setting are unchanged. There is no EYE SHADOWS switch: the
29 run did not show safely which pass to change, and a switch that does nothing is not added.

- **What the 29 run showed (all candidates):**
  - FLAT-ONLY 3, HEAD-ONLY 57, EYE-ONLY 21, mixed 160. The flat camera turned up only once, on
    4 of 559 frames, so the flat camera is now an unlikely cause of the sliding shadows.
  - Many exact matches are the head view (the game's own 110° view at the head). It may be what
    shadow cascades are fitted to, but nothing proves that yet.
  - The 256 raw-hit slots were full, and no `root_cbv` rows survived. So it was never seen
    which pass used which camera.
- **ADDRESS FINDER (still read-only, default OFF):**
  - Camera data found at a GPU address is watched: the copy target plus offset, up to 64 values.
    Eight GPU pages from the 29 run are also watched as `seed29`. Those addresses may move
    between runs, so they never count and are never used as live addresses.
  - A later root CBV whose 64 KiB window covers a watched value is a **bind** of that data. A
    camera hit in root constants counts as a bind too.
  - Each bind row keeps:
    - the call site (`rva`, `function_rva`), the root slot, and the value's offset in the bound
      buffer;
    - the camera (`cams`, `FLAT-ONLY` / `HEAD-ONLY` / `EYE-ONLY`);
    - the pass it fed: `depth_only` (no colour target, a depth target: likely a shadow map),
      `color`, `compute`, or `unknown`;
    - the colour-target count, whether there is a depth target, the viewport size, and the
      format and size of the last texture moved to DEPTH_WRITE on that thread.
  - Bind rows have their own 96 slots, so value hits can never crowd them out. When the slots
    are full, a new shadow candidate replaces a one-hit row.
  - Two new hooks, OMSetRenderTargets and RSSetViewports, only read the target count and the
    viewport. If they fail, passes read `unknown` and nothing else stops. Lookups are filtered
    by a lock-free page table and capped per frame (512 lookups, 48 stored binds).
  - Binds through descriptor tables are not traced yet (`SM2TODO`).
- **Report:** `address-finder-latest.txt` has `--- SHADOW PASS CANDIDATES ---`:
  - depth-only passes that bound this session's camera data come first;
  - then the other binds, with FLAT-ONLY and HEAD-ONLY first (a lighting or shadow resolve that
    uses a camera other than the eye's is the suspect);
  - then seed29 rows.
  The JSON has `bind` and `bind_summary` lines, with `update` set to 30. The HUD reads
  `DONE n +m, k SHADOW`, and the console line counts them.
- **Test:** stand close to a wall and turn the finder on. For its 60 s, tilt your head slowly up
  and down. Then, in the same spot, turn EYE LOD on for 1 minute and off for 1 minute.

## 40.1 Update 30.1: the finder runs its full window, and the update is shown at start

The Oct 23 run after Update 30 was still Update 29: its report said `port_version: "Update 29"`, so
the Update 30 files had not been copied over `C:\sm2-port`. That run also showed that the finder
stopped after about 2 s.

- **Full window:** while the bind trace is on, the finder no longer stops early once lighting, TAA
  and LOD each have a strong hit. Only the ~60 s window ends it (`finishNow`).
  - The finder's "frames" are stock view updates, about 6 per game frame. Its hard cap of 6300
    ended the 28.1 run after ~26 s, so the cap is now 42000 (~70 s at 600 a second).
  - A watched camera value now stays live for 48 of those views (it was 8, about one game frame).
- **Version shown:**
  - `launch-game-vr.ps1` and `build.ps1` first print `SM2 VR port: Update 30.1` in green.
  - The session console log's first line says the same.
  - The finder json has `"update": 30.1` and `port_version`; the txt header names both.
- Still read-only and default OFF, and all addresses remain candidates. Nothing else changed.

## 41. Update 31: shadows, testable in one session (TABLE BINDS, SHADOW CAMERA)

The 30.1 run (Oct 23) found no depth-only pass that bound camera data through root CBVs or
constants, and the eyes' shadow record was all zeros (`cascades [0,0]`, `lights 0`). The zeros can
mean SM2 moved the Remastered port's KeyLightContext offsets; they are not proof that the eyes have
no cascades. The player sees ground shadows end at a hard straight line a constant distance ahead
that moves with them while swinging. That is what the far edge of the last cascade looks like.

- **Plan A, ADDRESS FINDER (diagnostic, read-only, default OFF):**
  - Descriptor-table binds are traced now. The hooked calls were checked against `d3d12.h`:
    - command list vt 28 `SetDescriptorHeaps`, 31 `SetComputeRootDescriptorTable` and 32
      `SetGraphicsRootDescriptorTable`;
    - device vt 17 `CreateConstantBufferView`, 18 `CreateShaderResourceView` (buffers only) and 24
      `CopyDescriptorsSimple`.
  - Descriptors that view watched camera pages are kept in a 4096-entry map, capped at 8192
    descriptor operations a frame. A table bind looks at up to 4 mapped descriptors in a 64-descriptor
    window. A hit becomes a `bind` row with `api` `graphics_table` / `compute_table` and `table` (the
    descriptor's place in the table, from 1), carrying the pass kind and DSV as in Update 30.
  - `table_summary` gives:
    - binds per api and pass kind (unknown / depth_only / colour / compute);
    - the shader-visible heap (its size and increment);
    - table binds, and how many start at heap index 0;
    - a `bindless` verdict (`likely` when at least half start at index 0).
  - `depth_target` rows list every depth target that went to DEPTH_WRITE: format, size, transitions
    and depth-only binds. Shadow maps are usually large and square, or an atlas.
  - `ortho_box` rows hold orthographic matrices found in uploaded constants: the copy site, the
    offset in a 256-byte record, the box half sizes in metres and the hits. They are the cascade
    size candidates; the furthest box's reach is where the hard line would be.
  - The watched-value cap went from 64 to 256.
  - If the table hooks fail, only they turn off. The log says
    `descriptor-table hooks off (2740); table binds not traced`.
- **Plan B, SHADOW CAMERA (GRAPHICS page, live press, default OFF, never kept):**
  - What the shadow and lighting setup is fitted to can now be changed in the running game. Each
    press steps OFF -> STOCK NEAR -> FLAT CAMERA -> OFF.
  - The modes:
    - `OFF` is as before: the game view on the head, with NEAR CLIP's near plane.
    - `STOCK NEAR` keeps the game view on the head but gives it the flat camera's own near plane
      (used only when it is 0.5 cm to 1 m). The eyes keep NEAR CLIP. Cascade splits are usually spaced out from the near plane,
      so if the hard line moves, the splits follow it.
    - `FLAT CAMERA` leaves the game view on the flat camera behind the hero, as before the port
      aligned it. Culling also follows that camera, so this mode is a test, not a fix.
  - If the engine rejects a view built for a mode, it goes back to OFF on its own. The HUD shows
    `OFF (REVERTED)` and the reason, and that frame uses the normal view.
  - Every change is logged in `SpidyShadowCameraData`:
    - the time, the mode, and the frames the previous mode ran;
    - the console prints one line per change, and the report has `shadow_camera` with `log`,
      `segments`, `applied_frames` and `cascades_by_mode`.
  - The game view's own cascades are sampled every frame, as candidates (`SM2TODO` while their counts
    are 0). Calls to the 0x2dd8d20 fit are counted by view, as evidence of what it fits.
  - `live.txt`: `shadow_camera = off | stock_near | flat`.
- **Not built:**
  - **Rewriting the HEAD-ONLY view-proj records at upload 0x2ccbfc0.** Those records already are
    the head view. One record cannot serve both eyes, and rewriting it would break the passes that
    read it for the game view.
  - **A shadow distance knob.** No cascade-split or distance value has been confirmed in SM2 yet.
    A knob without one would be a switch that does nothing. Once `ortho_box` and
    `cascades_by_mode` name the value, a knob can follow.
- **Test (one session):**
  1. Start the finder.
  2. Swing until you see the hard line, then look down at it.
  3. Press SHADOW CAMERA to go through STOCK NEAR, FLAT CAMERA and back to OFF, about 15 s each.
  4. Note whether the line moves, and in which mode.

Before rebuilding, delete `C:\sm2-port\build`.

## 42. Update 32: capture-only — the ortho cascades and who fits them (ORTHO CAPTURE, FIT VIEWS)

- **What the Update 31 run (Oct 23) showed:**
  - SHADOW CAMERA applied every mode (11 changes, no reverts: OFF 4122, STOCK NEAR 810, FLAT CAMERA
    2097 frames), and the picture did not change.
  - The 0x2dd8d20 fit hook ran 4950 times, never for the game view or an eye ("other" every time).
  - The descriptor-table trace was blind: 17 M table binds were seen, but 0 descriptors were seen
    created or copied, so no table could be tied to camera data.
  - Cascade-shaped ortho boxes came through one upload site (copy at 0x2ccc294 in the flush
    0x2ccbfc0, record+64): half sizes 32 x 32 x 512, 64.3 x 64.3 x 343 and 146.8 x 146.8 x 783 m
    (candidates). Their sizes do not change, so the boxes look like fixed-radius cascades.
  - A FLAT-ONLY view rotation at cb+2800, uploaded by the same site, is confirmed.
  - Update 31 kept only the box sizes, not the matrices, so no centres, light directions or pitch
    dependence could be read.
- **Working idea (a candidate, not proven):** the cascade fit runs on the CPU from the main camera
  before the upload copy we hook, so neither SHADOW CAMERA nor the view hooks reach its input.
- **SHADOW CAMERA retired:** it is gone from the GRAPHICS page and from `live.txt`
  (`shadow_camera` is no longer a key), because it is a switch with no visible effect. The native
  modes remain, default OFF, and nothing sets them. The enum entry stays so control indices do not
  shift.
- **ORTHO CAPTURE (ADDRESS FINDER, read-only):**
  - On the live copy path, boxes that are cascade-shaped are captured. Cubes (all halves within 2%)
    and boxes over 2 km are skipped.
  - Captures come in groups: a 50 ms burst every 2.5 s, up to 6 matrices each and 24 groups (144
    captures), so a minute of head pitch shows up.
  - Each capture keeps:
    - the time and frame;
    - the copy destination's GPU address (page + offset), the matrix's offset in the copy, and the
      copy size;
    - all 16 floats;
    - the 256-byte record holding the matrix plus the next 128 bytes, for a split-array scan;
    - up to 6 game-module return RVAs above the copy (the flush and its callers);
    - the head and flat camera positions and facing at that moment.
  - The rows are `ortho_capture` and `ortho_capture_summary` lines in the finder report. The native
    report buffer grew from 256 KiB to 1 MiB to hold them.
  - `tools/ortho_dump.py` writes `reports/address-finder-latest-ortho.txt`, grouped by capture group.
    For each matrix it gives page + offset, the matrix, width / height / depth in metres, the light
    direction, and the centre relative to the head and flat camera. It also lists boxes that share
    one sun direction, any ascending float run in the record (a split array, flagged when it matches
    the box sizes), a per-size table of head pitch against how far ahead the box sits, and the
    upload call chains. `collect-reports.ps1` now takes `address-finder-latest*`, so the dump goes
    into send-me.zip.
  - It runs by hand too: `python tools\ortho_dump.py reports\address-finder-latest.json`.
- **FIT VIEWS (in `SpidyShadowCameraData`, which is now version 2):**
  - Every call to 0x2dd8d20 has its view read safely and classed as the flat camera, the head, other
    or unreadable (same camera = within 25 cm and 2 degrees).
  - The record keeps up to 8 distinct caller RVAs (the return address) and 8 distinct view pointers
    with counts, plus a sample every 2 s: view, flat and head position and facing, near and far.
  - The session report has these under `shadow_camera.fit_views`, all candidates.
- **Not built, and why:**
  - **SHADOW FIT PITCH 0** (a yaw-only camera for the fit) needs a confirmed call site that reads
    the main camera for the fit. None is known yet; FIT VIEWS and the upload chains are there to
    find it.
  - **SHADOW RANGE** needs the split array or the cascade radii in CPU memory. Neither has been
    found yet; the record scan is there to find them.
- **The upstream cascade-fit swap (SpidyVR 170fbc0, shadowLens) and SM2:**
  - Upstream copies a lens that covers both eyes into the view descriptor around the fit, then
    copies the original back. The port has the same swap at the SM2 candidate 0x2dd8d20, but in
    the Update 31 run it never ran for the game view or an eye. There were 4950 calls over about
    7000 frames, all for other views.
  - So a LEVEL (pitch 0) or SHARED LENS swap there would have nothing to act on yet. FIT VIEWS shows
    whether those other views are a per-frame copy of the head or flat view (render jobs work on a
    copy of their view). If they are, the swap can key on that copy.
- **Per-eye engine TAA (later, not built):**
  - Known in SM2 (from SM2VR, candidates):
    - sizeBuffers 0x2c6ebf0 (hooked; eye sizes);
    - the history step 0x2c9abf0;
    - enqueue 0x2c77810, with TemporalAa as command 0xb1 on job +0x18d0;
    - the profile flag, bit 0 of +0xcc8;
    - the render context's AA field, +0x424.
  - Missing (SM2TODO):
    - the TemporalAa handler (upstream 1842e10);
    - the buffer-kind mask table (upstream 3d11b18) and the target maker (upstream 1875270), needed
      so an eye's buffer set gets kTaaTarget0/1 and kTaaDepth0/1;
    - why SM2VR 0.0.3/0.0.5 turned eye AA off.
  - `SpidyEyeTemporal` still stores 0.

- **Test (one session, Virtual Desktop SSW Off):**
  1. Start ADDRESS FINDER.
  2. Swing until you see the hard line, then look down at it.
  3. For the 60 s, slowly pitch your head up and down, then turn it left and right.
  4. Run `collect-reports.ps1`.

Before rebuilding, delete `C:\sm2-port\build`.

## 42.1 Update 32.1: a graphics setting changed mid-session no longer ends VR

- **What the run32b session (Oct 23) showed:**
  - The user turned Ambient Occlusion from Off to SSAO in the game's settings while ADDRESS FINDER
    ran. The game logged "New display settings" at 17:02:40.8 (Shadow Quality still Medium, no
    swapchain resize).
  - XR frames stopped advancing about 2.5 s before VR stopped, while the game applied the setting.
  - VR stopped with "GPU eye bridge: 3501; then: Native GPU bridge did not stop cleanly". 3501
    means "a different eye image was registered for an eye that already had one". The game had
    rebuilt its render targets, our two eye images among them. The "then" part is the same fault
    reported again by the bridge's stop, not a second failure. The GPU fence completed 8720/8720,
    and no device-removed message was logged.
  - The flat window came back, then the game froze until it was force-closed. The logs end there,
    so the freeze is not pinned down. The likely candidate is our teardown racing the game's
    rebuild. 32.1 avoids the teardown entirely.
  - ADDRESS FINDER was not involved: fault 0, still running, 222 confirmed in 4064 frames, and its
    partial report saved. It is unchanged.
  - Shadow Quality was Medium in every session so far (run31, run32, run32b). Medium has the
    shorter cascade range, which is a candidate for the hard shadow line. Try High.
- **The fix (`include/spidy/eye_swap.hpp`, `src/native_eye_gpu.cpp`):**
  - **Same kind of image:** when an eye's image is replaced by one with the same size, format,
    samples and relevant flags on the same device, the bridge takes it over.
    - The old image stays referenced until the bridge's GPU work submitted before the swap has
      finished (fence).
    - The anti-aliasing pass's view of that eye is rewritten only when none of our work is in
      flight.
    - Eye pairs are skipped until then, and state transitions for the old image are ignored.
    - The console says "A graphics setting rebuilt the eye images; VR picked up the new ones and
      carries on".
  - **Different kind of image (or another device):** the eyes pause with code 3503, which is not
    fatal.
    - No new eye pairs are made and the copy wait wakes up.
    - VR stays up and the headset shows the game screen. The controllers drive the menus as on any
      game screen.
    - A notice is drawn over the screen: "A GRAPHICS SETTING REBUILT THE EYE IMAGES / VR SHOWS THE
      GAME SCREEN UNTIL A RESTART / RESTART THE GAME TO GET 3D BACK".
    - The console carries the same message with (3503).
    - If drawing the notice fails, only Spidy's drawings switch off, with one line.
  - 3501 is no longer raised. 3502 (the first eye on another device) is unchanged.
- **Tests:**
  - Core test "Update 32.1: a graphics setting's new eye images are taken over or pause the eyes":
    which images count as compatible, when an old image may be freed (never on a removed device's
    fence), notice length, and that the notice stays on the image.
  - `tests/eye_swap_tests.py` checks the sources.
- **Try it:** set Shadow Quality to High before starting VR. In a session, toggle Ambient Occlusion
  (or another graphics setting). VR should stay 3D with the "picked up" console line, or show the
  game screen with the notice. Either way it should not go flat or freeze.

## 43. Update 33: SHADOW DISTANCE, EYE AA, a smaller desktop view (and why there are no shared shadows)

One cumulative update on 32.1, which keeps 32.1's eye-image-swap survival.

**SHADOW DISTANCE (GRAPHICS page, slider).** Hard sun shadows end a few metres from the hero because the
game's sun cascades are small boxes (run32 ORTHO CAPTURE half sizes about 24.6, 28.9 and 56.2 m). No CPU
value that sizes them is known for SM2. The candidate writers seen so far (0x2abe8dd, 0x2c904aa/0x2a8c76e,
0x2e01533/0x2e01a9c, 0x2c960c9/0x2b0912e) are leads only, so the slider starts locked ("NOT FOUND").
- **Search.** ADDRESS FINDER starts it: a read-only search of this session's memory
  (src/native_shadow_distance.cpp, a background thread with budgets). It looks for floats or doubles equal to
  a live box's half size, width, depth and the like, or three values in the same proportions as the
  cascades. Values that do not hold still are dropped. Live boxes come from ADDRESS FINDER's
  upload watch, using the sun-box test in shadow_distance.hpp.
- **Range.** 0 = the game's own reach. 1-100 = car lengths (4.5 m each, 450 m at the top), never below a
  quarter of the game's reach. The text shows metres and cars.
- **Apply.** Moving the slider writes a trial value with an SEH POD compare-exchange, then checks the next
  frames' sun boxes. If they don't scale, the value goes back and the next candidate is tried. If none
  scales: NO EFFECT, and everything is put back. A write fault, vanished boxes, wild boxes, the game
  resetting the value, or VR ending puts it back with the reason in the subtext.
- **Trade-off.** The shadow maps keep their size, so the farther the reach, the coarser and blurrier every
  shadow. The subtext says so.
- **Record.** `shadow_distance` in game-vr.json (tools/shadow_distance.py) lists every candidate. Only one
  marked `confirmed this session` (its change moved the boxes) is a finding, and only for this game build.

**EYE AA (GRAPHICS page, toggle, live, default OFF, never kept).**
- **Tier 1 (port-owned, in).** Each eye's lens moves by a Halton(2,3) 8-step sub-pixel offset every frame
  (eye_taa.hpp). The existing reprojected history pass blends the frames with its neighbourhood clamp, at
  least 80% history while on, and the clamp is kept tight. The history's homography and the headset's
  projection layer get the same shifted lens, so the picture doesn't wobble. FXAA stays on.
- **Cost.** The button shows the history pass's GPU ms, expected under 1 ms (about 1-3 FPS at
  90 Hz).
- **Limits.** Without eye depth, things that move on their own (cars, people) are only clamped, not
  reprojected, so a slight softness or trail on them is expected.
- **Failure.** A history-pass fault turns EYE AA off with "Failed: <reason>." (one log line
  "EYE AA: off, ...").
- **Tier 2 (engine per-eye TAA, not in, SM2TODO).** The TemporalAa handler (upstream 1842e10), the kind mask
  table (3d11b18) and the target maker (1875270) have no high-confidence SM2 addresses.

**Cheaper hidden flat view.**
- **Already in place.** The game's own view must keep rendering in SM2 (occlusion, key-light shadow setup,
  auto-exposure). Resting it (flag 32) loses the device, as in SM2VR, so that view isn't skipped. It
  already runs in a 1920 x 1080 window (vr_display.py), not the desktop's size.
- **New: `-SmallerDesktopView` (launch option, next launch).** Gives a 1280 x 720 window: 44% of the
  pixels. Next to two 3072 x 3264 eyes a frame draws about 5% fewer pixels (3.5% at 3840 x 4080).
  Realistically that's a few percent of GPU time where the GPU is pixel-bound. The cost: the HUD panel is
  drawn at the window's size and gets softer. Default is unchanged. The game reads the window at launch,
  so the console says so. No other skipping of unseen work was provably safe, so none was added.

**SHARED SHADOWS (upstream eyeShadows bit 1): left out.** Sharing hands the second eye the first eye's
scene-submit parts (+0x18 light set, +0x28 KeyLightContext, +0x30) at Remastered offsets. In SM2 those
offsets read nothing: run32b's SpidyShadowData shows 1946 submits per eye with 0 cascades, 0 lights and
may_take 0, and the game view's cascades are 0 at the same offsets. The parts handed over would be whatever
SM2 keeps there, which is how SM2VR 0.0.4's render thread "read a part not ported" and crashed. Fixing it
needs SM2's scene-submit result layout, which is unconfirmed, so a switch would be a fake or a crash.
SpidyEyeShadows still stores 0.

**Tests.** core_tests (SHADOW DISTANCE / EYE AA pages, mask, help, ranges, box test, targets, verdicts,
jitter, lens shift), tests/shadow_distance_tests.py, vr_display_tests (smaller view), seh_safety (the new
file), version strings.

## 44. Update 34: ENGINE TAA (the game's own TAA on each eye, opt-in, live, plan B built in), EYE AA Tier 1, SHADOW DISTANCE fixed

One cumulative update on 33. Found from the user's Spider-Man2.exe by offline disassembly. In this section,
**confirmed** means read in the disassembly; **candidate** means inferred and still to be proven in a run.

**What the exe shows (confirmed).**
- **TemporalAa.** Render command 0xb1, handler 0x2c2dc60. It runs only if the frame's AA
  ([0xc2c6a10], settings global 0xc2c64a0 + 0x570) is 1 and the job ([0xc2c6af0], +0x650) has a non-zero
  jitter (|job+0x428| + |job+0x42c|). Its targets come from 0x2c98240: the job's +0x18e0 buffer set,
  +0x12648 / +0x12a18 (by a flag), else nothing. The game enqueues 0xb1 for every view.
- **The kind mask table.** 0x5d65408 (.rdata) = {0x7ffff, 0x7ffff, 0x3fcc8 x 6}. Target maker 0x2c6eec0
  makes a target only if its kind bit is in the table's entry for the buffer set's type. The TAA targets
  (kTaaTarget 160, History 161, Depth0/1 164/165) have kind 4, which entries 2..7 (the offscreen sets the
  eyes are) do not have. That is why eyes never got TAA.
- **The game AA.** 0xa164900. It is read by the target maker; the TAA depth targets need it at 1 or 5..7.
  The user's menu maps to it as 0 to 0, 1 to 2, 2 (TAA) to 1, 3 to 5, 4 to 6, 5 to 7.

**ENGINE TAA (GRAPHICS page, press, live, default OFF, never kept; engine_taa.hpp, stereo_probe.cpp).**
- **Preparation.** Launch with `-EngineTaa` (options 0x800000; SpidyEyeTemporal bit 0x10). When each eye's
  buffer set is made, ENGINE TAA OR's kind 4 into the table's entries 2..7 and calls the game's initialiser.
  Then it puts the table back exactly. This is done under a lock, only if the table matches the exe's
  bytes, with VirtualProtect around the write. Otherwise the table is left alone and the setting shows
  "this game build differs".
- **On.** The press is refused, with the reason as subtext, unless both eyes have the TAA targets
  (eye +0x18e0 or +0x1e38 set, +0x12648 / +0x12a18). Once on:
  - copyJob writes the eye job's settings +0x570 = 1;
  - the immersive eye lens gets a Halton jitter (2 x (h - .5) / eye size, 8 phases), the units Remastered
    uses (a **candidate** for SM2);
  - the eyes render at their full size, with no upscaling;
  - the game camera's own jitter and the eye's are logged every 2 s ("ENGINE TAA: on; ...") so the units can
    be checked.
- **Off by itself (plan B).** It turns off if VR ends or an eye loses its targets. It also turns off if the
  FPS guard sees eye pairs per second fall below half within 3 s (1 s to settle, then 2 s, against the
  2 s before). After a target loss or a slowdown, EYE AA goes on in its place. The two are exclusive:
  turning one on turns the other off.
- **Texts.** ON / OFF / OFF (RESTART) (not launched with -EngineTaa, or the game AA is not TAA) /
  OFF (FAILED). Subtext "Off: <reason>." Help: "The game's own anti-aliasing on each eye; turns itself
  off if it fails."
- **Candidates and risks.**
  - Whether SM2's TemporalAa resolves into the image the eye copies and keeps a separate history per
    eye. The two eyes are separate views with their own buffer sets, so it should.
  - Whether the jitter units match.
  - Motion vectors for the eye views.
  If the picture is wrong, press it off; nothing persists.

**EYE AA Tier 1.** 16 Halton steps (was 8), at least 88% history (was 80). While on, the 3 x 3 clamp gives
at least 25% of the range each side (eye_taa::clampGive; still none while the head turns). This targets the
edge and far-building flicker left in run 33. The SHARPEN default is unchanged, because it is the user's
saved setting.

**SHADOW DISTANCE** (commit 67b908a).
- Only heap or writable module data is written. Update 33 could write a code constant, which is why it
  "turned itself off" in run 33.
- Changes only grow the reach, and each trial is checked against the box it matched.
- An EnvLightingDef search (RegLodRange +0x6c, TotalShadowRange +0x78: reflection fields confirmed; heap
  objects candidates).
- Data v2 trial log.
- Also: `far` in shadow_distance.hpp was renamed (`far` is a Windows macro; any TU including both broke).

**Test advice.**
- Set the game's Anti-Aliasing to TAA and Shadow Quality to High (not Ultra) before launch, then launch with
  `-EngineTaa`.
- In one session, A/B: EYE AA on, ENGINE TAA on, both off.

## 44.1. Update 34.1: -EngineTaa no longer starts the game flat (Game XR start: 1001)

**Cause (confirmed, run34 logs).** The console shows the ENGINE TAA line, then `VR did not start
(starting VR)` and `Spidy VR unavailable: Game XR start: 1001`. `SpidyXrStart` (game_xr.cpp)
refuses a configuration with 1001, and one of its checks was `config.options > 0x7fffff`, unchanged
since Update 9. Update 34's `-EngineTaa` is options bit 23 (0x800000), past that limit, so every
`-EngineTaa` launch was refused before anything else ran (the table prep never ran). Without
`-EngineTaa` Update 34 starts as before. DLSS off, TAA and Shadow Quality High had nothing to do with it.

**Fixes.**
- game_xr.cpp: `knownOptions = 0xffffff`; option bits past it are dropped and logged, never a refusal.
- run_game_vr.py: if SpidyXrStart still refuses with 1001 and `-EngineTaa` was asked (an older DLL),
  the launcher says so and starts VR once more without the ENGINE TAA bit; a 1001 now says the
  launcher and DLL are from different builds.
- Zip times: `tools/make_update_zip.py` dates every entry two days before now (zip times carry no time
  zone; 34's were this box's UTC, hours ahead on a PC behind UTC, so ninja said "build.ninja still
  dirty after 100 tries"). `tools/build.ps1` also re-dates any future file to now before it builds,
  and after a build prints the VR module's path and time.
- tests/launcher_options_tests.py: every bit the launcher sends is within `knownOptions`.

## 44.2. Update 34.2: EYE AA's swing trail cut hard (live TRAIL slider), ENGINE TAA jitter plan B and telemetry, SHADOW DISTANCE settable while searching

**What run341 showed.**
- SHADOW DISTANCE (confirmed): the record ends `ready`, `want_cars 0`, 0 trials, and the event ring holds only
  state SEARCHING, three search steps and state READY. No testing, apply or put-back event ever ran, so the
  slider's value never reached the search thread. The cause is a candidate: the slider ignored input while
  SEARCHING (and `setState` cleared the value on every non-live state), so a value set early was thrown away.
  Also the game's own reach was 195.7 m (about 43 cars): any value up to 43 cars does nothing ("already reaches
  that far"), and the top was 100 cars (450 m). The 16 EnvLightingDef candidates were never tried.
- ENGINE TAA (candidate only): its state and jitter went only to OutputDebugString, so the report has none.
  The ON windows are inferred from EYE AA's blending stopping (about 61–96 s, 110–120 s, 145–156 s). The
  shaking far buildings fit double jitter, wrong units or no per-eye motion vectors. The evidence does not
  choose between them, so 34.2 ships live variants and records the game camera's jitter size.
- EYE AA (confirmed by the user): the edges were clean, but there was a heavy trail while swinging. The history
  weight only fell with the head's turn, never with travel.

**Fixes.**
- EYE AA motion falloff (eye_history.hpp `movedWeight`, `movedLoosen`): the history kept falls from the still
  weight at 0.02 m (or 0.01 rad) between pairs to TRAIL% of it at 0.20 m (0.12 rad), and to zero past
  `travelCut` (0.65 m at TRAIL 10). The clamp's give is gone at half swing speed.
- EYE AA TRAIL slider (GRAPHICS page, past the mask, 0–100 step 5, default 10): live while dragged
  (`SpidyEyeTrail`, error 3994). live.txt `trail` (aliases `motion_blur`, `eye_trail`) is kept from session to
  session. The report's eye_history now has trail, moving pairs, motion cuts, mean/max travel and least kept
  (HistoryData v2, 96 bytes).
- ENGINE TAA JITTER (GRAPHICS press OURS → GAME → PIXELS → NONE; live.txt `taa_jitter` 0–3, `taa_jitter_scale`
  0–200%; errors 4603/4604). It is live and never kept. OURS is as in 34 (2·halton/size, clip units). GAME passes
  the game camera's own +0x428 values. PIXELS passes the halton offset unscaled (render-pixel units). NONE sends
  no jitter. It only applies while ENGINE TAA is on and EYE AA is off.
- `SpidyEngineTaaData` (engine_taa.hpp Data, 144 bytes): state, reason, mode, scale, on/refused/off counts,
  frames on, newest and largest jitter (ours and game camera), and targets per eye. The launcher prints a line
  when the state or mode changes and puts `engine_taa` in the report.
- SHADOW DISTANCE: the slider takes a value while SEARCHING (`sliderMoves`); it is kept and tried when the
  search ends. The top is now 200 cars (900 m). With "already reaches", the subtext says the game's reach and
  how many cars to go past. Each value asked is said and recorded (event 6 `asked`), and the console line shows
  `asked N cars`.

**Reading the next logs.**
- `ENGINE TAA: ... game camera 0.5/0.5` means the game's jitter is in pixels, so OURS was wrong in units. Try PIXELS.
- If NONE still shakes, the cause is not the jitter: it is the history or motion vectors. Use EYE AA instead.
- If GAME stops the shake, OURS was jittering on top of the game's (double jitter).

## 45. Update 35: FAR SHADOWS (the shadow line 1–2 blocks ahead), EYE AA's real cost, a harder swing cut, TAA JITTER press OURS↔NONE

**The symptom (run342 and the user).** A hard, straight shadow line 1–2 blocks ahead. Past it, everything is
fully lit, and the line slides with head pitch. SHADOW DISTANCE (130 → 2000) changed nothing.

**Cause (from disassembly of Spider-Man2.exe 2.810.0.0; strong candidate, not yet seen in game).**
- Confirmed (code): SubmitScene 0x2d01b30 builds each view's key-light cascades (a 0x5060 object) in 0x2a938b0.
  That function calls the fit 0x2a8dc80 (rcx = cascades, rdx = view, r8, r9 = settings).
- Confirmed (code): the fit calls IsActiveView 0x2c991a0 at 0x2a8e145 (`e8 56 b0 20 00`). Only for the active
  view does it fetch the far cascades from the far cache 0x2ad11b0. It writes their count to cascades +0xe and
  sets LodConsts (+0xc0) `.z` = base count and `.w` = base + far. FarLodConsts (+0xd0) is the cache centre
  relative to the view.
- Confirmed (code): the shader fades a pixel to unshadowed past the base cascades' box when LodConsts.w ≤ .z.
- Confirmed (port): the eyes are never the active view. The active view is the head/game view (ViewContextManager
  +0xf8). So each eye got base cascades only, fitted to its own frustum. The result is lit past a box that moves
  with the head, and no reach setting can change it. SHADOW DISTANCE acted on the base fit, so it could not help.
- Confirmed (code): the old SM1-era hooks are wrong for SM2. `fitCascades` 0x2dd8d20 never runs for the eyes
  (Update 32 already saw that), and the SM1 impostor IsActiveView callers do not match SM2's bytes (they are
  disabled). SM2's impostor callers are candidates (0x2a6c34b/35b, 0x2a6c46c/47c; the fifth is SM2TODO). They
  may be behind the vanishing buildings, which Update 35 does **not** fix yet.

**FAR SHADOWS (GRAPHICS page, top; press cycles OFF → EYES → EYES+CACHE; default EYES; live, never kept).**
- It is hooked only when the fit's entry and both IsActiveView calls match the known bytes. If they differ, the
  page shows `OFF (N/A)` and nothing is changed.
- EYES: IsActiveView answers "yes" for an eye only when the caller is the fit's ask (return 0x2a8e14a). The far
  cache then sees the eye as not active and hands back the cascades the active view last made. If those are 2 or
  more frames stale, it makes them itself around the eye.
- EYES+CACHE (plan B): the left eye is also "active" inside the far cache (return 0x2ad123d), so the cache is
  refitted around the left eye.
- Auto-revert: the csmFit hook counts each eye's fits and how many had far cascades (`SpidyFarShadowsData`,
  256 bytes, "FARS" v1). After 240 eye fits with none:
  - EYES → EYES+CACHE if the game view had far cascades;
  - otherwise → OFF (reasons: no far shadows / the game made none here / no eye fit seen).
  - A wrist press that halves the frame rate turns it OFF (FPS guard).
- live.txt `far_shadows` 0–2. Console: `FAR SHADOWS: EYES, eyes far cascades a/b (base c/d), game view far e`.
  The report has `far_shadows`.
- Candidates (risks): other code that reads "active" for an eye at that one call site (none found besides a
  harmless base-count fade); the GPU cost of drawing far cascades per eye; cache thrash in EYES+CACHE.

**SHADOW DISTANCE** has left the page (it had no visible effect, which is now explained above). Its code stays.

**EYE AA cost (confirmed in run342).** The hologram showed only the newest sample: 6.33 ms was a spike, and the
mean was 0.337 ms. The hologram now shows the median of the last 31 passes (`eye_taa::MsMedian`).

**Swing smear.** In run342 the trail was already 0 and pairs over 0.2 m were fully cut. The smear that is left
comes from slow and medium motion (mean 0.043 m per pair). The ramp now runs over 0.005 → 0.06 m and
0.005 → 0.06 rad (was 0.02 → 0.20 m and 0.01 → 0.12 rad), and TRAIL defaults to 0.

**TAA JITTER.** The press now toggles OURS ↔ NONE. GAME and PIXELS are dropped from the press because they made
no difference, but live.txt can still set them.

**Test (one session).**
1. Start as usual, swing outdoors in daylight. Look ahead past 2 blocks: shadows should continue.
2. The console shows `FAR SHADOWS: EYES, eyes far cascades N/N` with N > 0. That means it works.
3. A/B: press FAR SHADOWS on GRAPHICS (EYES → EYES+CACHE → OFF) and compare. OFF should bring the line back.
4. If it says `back on its own: ...`, keep the report: it says which part failed.

## 46. Update 36: EYE BLOOM, SHARED LOD, the impostor stand-in on SM2's addresses, the body calibration panel, the health and focus ring, the 2000s Raimi hologram

Everything here is live (no restart), never kept between sessions, and each new graphics press has its plan B in
the same build. Nothing here can start the game flat: a hook whose bytes differ shows `OFF (N/A)` and changes
nothing, and the art falls back to the plain panel. Update 35's FAR SHADOWS (section 45) is included unchanged.

**Why the eyes looked different from the flat game (docs/main-view-gates-audit.md).** The audit corrected
Update 25's cause #1: GlobalWorldCB is built per render job, so each eye already has its own. What does differ is
the view settings 0x2c27ec0 builds for a view that is not the active one: bloom (+0xd8) is zeroed (0x2c29102), and
each view's level-of-detail scale comes from its own lens (0x2c9a000 writes +0x1b24; +0x1b28 multiplies it).

**EYE BLOOM (GRAPHICS; press OFF → COPY → GATE; default OFF).** Subtext: "Gives both eyes the game view's bloom
glow; GATE is the plan B."
- COPY: before copyJob 0x2d03240 runs for an eye, the eye's +0xd8 takes the active view's ([0xc2cae80] +0xd8).
- GATE (plan B): the 2-byte jump at 0x2c29102 skips the "not active, so no bloom" zeroing; `-no_bloom` still wins.
- Auto-revert after 240 eye jobs: COPY → GATE when no eye settings came through; → OFF when the game view has no
  bloom either, GATE left the eyes' bloom at 0, or a write failed. The subtext says which.
- Candidates: +0xd8's type (copied as raw bits) and how visible it is.

**SHARED LOD (GRAPHICS; press OFF → GAME VIEW → HIGHER; default OFF).** Subtext: "Eyes use the game view's detail
distance; HIGHER is the plan B."
- GAME VIEW: after 0x2c9a000, an eye's +0x1b28 is set so its scale equals the game view's ([0xc2cae70]).
- HIGHER (plan B): the larger of that and the eye's own, never less detail than now. OFF puts 1.0 back.
- Auto-revert: GAME VIEW → HIGHER if it would halve the eyes' detail distance; → OFF if the game view's scale is
  unusable, no eye came through, or a write failed. EYE LOD (Update 28) stands aside while it is on.
- Aimed at buildings that vanish, pop or shake at the edge of view (LOD fade, audit #8).

**Impostor stand-in on SM2's addresses (automatic).** The rest-mode stand-in (the left eye answers "active" for
the impostor code while the game's own view rests) had SM1's addresses, so it never switched on in SM2. It now
uses the impostor submit job 0x2a6af90, the HQ model launchers 0x2a6c346/356/467/477 and reachLast 0x2a6bded,
each byte-checked first; any mismatch leaves it off, as before.

**Body calibration panel.** The procedural figure is gone: the panel shows the user-supplied Spider-Man picture
(tools/calibration_art/hero.png, packed into the compiled-in atlas), its aspect kept, with a gold glow behind him
that grows with each step done. If the atlas cannot be decoded the plain panel is shown instead; no crash, no
flat screen. hero.png is the user's own picture, not Spidy's work: see THIRD_PARTY_NOTICES.md.

**Health and focus ring (under the left wrist).**
- Both crescents now drain from the wide right end toward the thin left tip, and taper more (about 2 mm at the
  left tip, 16 mm at the right).
- Focus is Spidy's own slow-motion meter (SlowMotion, the same value the old wrist meter drew), updated every
  headset frame, so it drains and refills smoothly. Confirmed in the code; no game address is involved.
- Health is HeroHealth hit points over their most (static candidates, not yet seen live); a hit is eased over
  about 0.2 s (60 ms time constant) so it slides instead of jumping. Unread health shows N/A.

**2000s Raimi hologram.** A soft blue halo around the panel, a faint glow on text and borders, and a very light
flicker, all as a few extra quads (no full-screen blur). Other themes are unchanged.

**Test (one session).** See SM2-VR-Update-36.pdf: shadows ahead, buildings at the edge of view with SHARED LOD,
bloom on lights with EYE BLOOM, then the calibration panel, the ring and the theme.

## 47. Update 37: gadget gestures, hand hits, lock-on, web physics, hand-over-hand wall crawl, heal, the MIRROR, the view gates (BLOOM FX, LOD FADE, LOD REACH, IMPOSTORS, EYE OCCLUSION), HISTORY CUT, AA QUALITY, the flat ring, the WORLD LOG, BLOOD SPLATTER, the CONTROLS page and the performance pass

Every setting below is live (live.txt or the hologram) and has a one-sentence subtext, except EYE OCCLUSION,
which can only change when an eye view is made and so shows a restart notice. Each graphics change carries its
plan B in the same build; a hook whose bytes differ shows `N/A` and changes nothing, so nothing here can start
the game flat or crash it (each feature catches its own failure and turns itself off with a note). Addresses
are from Spider-Man2.exe 2.810.0.0 and are marked CONFIRMED (disassembly / strings / RTTI) or CANDIDATE.

**New defaults (run36's closing settings).** FAR SHADOWS EYES+CACHE, EYE BLOOM COPY (with BLOOM FX LENS), SHARED
LOD HIGHER, EYE AA FXAA, history 80, TRAIL 0, sharpen 140, ENGINE TAA jitter NONE (3), theme 2000s Raimi, web
width 1.2, web speed 70, reticles 0. `taa_jitter` itself is not applied at the start (Update 34.2's rule stands);
`taa_jitter_scale` (0–200 %, 100 as made) is.

**View gates (include/spidy/view_gates.hpp; docs/main-view-gates-audit.md).**
- BLOOM FX (OFF / LENS / LENS+; default LENS): zeroes, in the eyes' own view settings only, the lens-fixed groups
  the game itself zeroes for some views (0x2c293ca..0x2c293fa; LENS+ adds 0x2c29113..0x2c291d6). CONFIRMED safe
  combinations; which effect each field is remains CANDIDATE. Plan B: EYE BLOOM OFF and the game's Lens Flares Off.
- LOD FADE (OFF / LEFT / BOTH; default LEFT): the left eye (BOTH: both, plan B) answers "active" at 0x2b4f9b8 so
  trees and props fade in instead of popping. N/A when the bytes differ; back to OFF if no eye reaches the call.
- LOD REACH (100–300 %, default 100): scales the eyes' detail multiplier (+0x1b28 × +0x1b24, clamped 0.25..4).
- IMPOSTORS (on/off, default on): the eyes' stand-in for the far-building impostors, now live.
- EYE OCCLUSION: giveOcclusion 0x2c9b8e0 runs only when an eye view is made: applies at the next VR start
  (restart notice). The query cap [0xc35950c] stays out: unconfirmed.

**Eye image.** HISTORY CUT (25–400 %, 100 as Update 35): the motion EYE AA's history fade-out sees is divided by
cut/100, so lower cuts the history sooner (fewer trails), higher keeps it longer (smoother, more trail). AA
QUALITY 1 STRONG: FXAA 3.11's strongest sub-pixel and edge settings (an "SMAA-like" option without a new shader;
true SMAA is not shipped).

**The ring.** RING FACE 1 (default) lays it flat on the web shooter and fades it as it turns away; 0 faces the
eyes. RING OFFSET 0–150 mm, RING TILT −60..60°. Health: HeroHealth vtable 0x541edd0, hit points +0xd0, most
+0xa0 — CANDIDATE; the HLTH record logs whether it falls on a hit and rises on a heal (status 3 = CONFIRMED live).

**Combat by gesture (combat_input.hpp), sent as the game's own pad input.** Web Shooter = tap the trigger; Web
Grabber = hold + yank back; Upshot = aim low + hold + swing up; Ricochet Web = trigger + grip + flick; Sonic
Burst = both hands forward and close + both triggers. Gadget = RB held + a face button per gadget (grabber_slot,
sonic_slot, upshot_slot, ricochet_slot: CANDIDATE buttons, fixable in live.txt). A trigger hold that makes no
gesture does nothing; Impact Web is not used. HAND HITS: a fast free hand = X (the game aims it). LOCK-ON: the
wrist aim steers the lock-on, sticky (aim_cone, lock_stick; CANDIDATE). Per-enemy hits (game_punch) are not ported.

**Movement.** WEB PHYSICS (default on; web_wiggle, web_bend): strands fly, ripple and sag; preview in
preview37. WALL CRAWL 1: the game's own crawl lays the body on walls; HAND CLIMB: pull hand over hand
(climb_pull, climb_grip), the stick as plan B. HEAL: right stick down held heal_hold_ms (default 250 ms).

**MIRROR (vr_mirror.hpp; default off).** The tracked head and hands as a Spider-Man figure, mirrored, body only;
PLAYSPACE (default) or WORLD anchor; RECENTRE and dismiss in the hologram; distance, size, spot, style (PANEL =
plan B). Zero cost when off. Not the game's rendered body (a third game view is not evidenced).

**WORLD LOG (world_trace.hpp; on by default; world_log, npc_log, spawn_log, mission_log).** Read-only, silent,
rate-limited scans of the CONFIRMED component registry (table 0xc2d3d20, count 0xc2d3d40) for NPC dialogue,
spawns/despawns and mission states, journalled to reports/world-trace-*.jsonl (in collect-reports' send-me.zip).
Stops itself on a fault or slow scans. Subtitle text is N/A (ShowSubtitle 0x53ee1d8 / HideSubtitle 0x53ee1c8 are
candidates). See docs/spawn-and-mission-research.md (CONFIRMED vs CANDIDATE); the teammate bot "Spider-Man Hero
Expansion" may use this data.

**BLOOD SPLATTER (blood_splatter.hpp; default OFF; 0–200 %, 150+ BRUTAL).** Short splashes (< 0.5 s, capped),
our own billboards, zero cost off. Confirmed per-enemy hits are not ported in SM2, so it fires on the plan B: a
hand hit while the WORLD LOG sees an enemy within 3 m (so it needs WORLD LOG on; the CONTROLS page shows N/A
otherwise). Web/gadget impacts do not splash. Blood stains on enemies are NOT shipped: the exe has BloodHitEffects,
BloodSplatterEvent, BloodSpout, DamageStateManager, WetnessTracker and DecalSystem::ProcessMeshSplatDecal (all
CANDIDATE), but the event's raise function, the live "Blood Enabled" flag and the decal entry are not evidenced.

**CONTROLS page (hologram page 6).** A plain short line for every motion and gesture control, greyed and marked
OFF or N/A from the live settings, in every theme including 2000s Raimi.

**Performance pass (perf37.hpp; PRF7 record, update37_records.py).**
- Per-part CPU ms in every log: the frame's prepare in seven sections (mean/peak) and the slowest frame over
  100 ms named by its slowest section (to find the 1.7 s prepare spike); the overlay split into world+holo,
  wheel/phone and drawing; holo build vs reuse; blood, mirror and WORLD LOG scan cost.
- HOLO REUSE (holo_reuse, default ON): the hologram's triangles are rebuilt at most every other frame while the
  panel sits still (under 3 cm / 3°) and nothing on it changes (page, scroll, hover, press, no pointer); in between
  they are carried rigidly onto the panel's new place. OFF = every frame, as before.
- copy_gpu is measured from the copy's submission until the CPU sees its fence: it is mostly the wait behind
  the game's own frame on the same queue, not GPU work. The log now splits it into work (the sharpen pass) and
  wait. Nothing is changed there.
- The hidden flat window still renders the left eye's copy: `-SmallerDesktopView` (section 43) shrinks it.

**Test (one session).** See SM2-VR-Update-37.pdf.

## 47.1. Update 37.1: Update 37 builds with Microsoft's compiler again (C2733 SpidyEngineTaa), and its warnings are gone

**The failure.** Update 37's automatic ENGINE TAA start (`SpidyEngineTaa(3)`) declared the exported
`extern "C" SpidyEngineTaa` inside stereo_probe.cpp's anonymous namespace so the frame code could call it.
GCC and clang take that for the same C function; MSVC does not: it names the declaration
`` `anonymous-namespace'::SpidyEngineTaa `` and stops at the real export with **C2733** ("you cannot overload
a function with extern "C" linkage"). The fix: both forward declarations (`SpidyEngineTaa` and `SpidyEngineTaaNow`)
now sit at file scope, in the file's own `extern "C"` block, marked `__declspec(dllexport)` like their
definitions, so there is one function, one export. Nothing the game sees changed.

**Warnings fixed (all builds are /W4).**
- C4459 (a local hides a global) in native_eye_gpu.cpp: `copyTiming`'s `pair` → `pairOut`,
  `eyePassesGpuMs`'s `known` → `allKnown`, `readPixels`'s `width`/`height` → `imageW`/`imageH`.
- C4701 (possibly uninitialized) in world_trace.cpp: the three `gaze` outputs start at 0.
- The same kind found by a clang check with `-Wshadow-all` across every source: stereo_probe.cpp (`command`,
  `eyes`, `jobs` hid namespace globals), game_xr.cpp (`fight`, `made`), health_ring.cpp, wrist_hud.cpp,
  game_pad.hpp, native_address_finder.cpp, two preview apps and the tests; plus unused names
  (world_trace's `ownerRecord`/`ownHandle`, game_prompts' buffers, a test's `body`), an unhandled
  `Kind::device` in game_grab.cpp's switch, and a missing `<initializer_list>` in controls_guide.hpp.
- `/bigobj` for MSVC builds: core_tests.cpp and game_xr.cpp keep growing, and this keeps them under the
  object file section limit (C1128). It changes no code.

**Audit of the other Update 37 files for MSVC-only problems:** no other extern "C" declaration inside a
namespace is new (native_address_finder's two, from Update 27, have always built); no VLAs (array sizes are
constants), no GCC builtins or attributes, no deprecated CRT calls, designated initializers only in declaration
order (C++20), and the 2.4 MB calibration picture is embedded as a byte array of 32-byte lines, not a string
literal, so C2026 cannot occur (it built in Update 36).

**Test.** Build as usual; the game should start in VR exactly as Update 37 describes (SM2-VR-Update-37.pdf).

## 47.2. Update 37.2: settings that start on, SM1 web feel, COLOR / ATMOSPHERE, NPC NEAR FADE, experimental FOVEATED

**Starts on, every time (tested).** EYE AA (export `SpidyEyeAaStart`, live.txt `eye_temporal` 1), FAR SHADOWS
EYES+CACHE (2) and sharpen 140 start on with a fresh install and over an existing live.txt: the launcher runs
`migrate_372` once (it sets them, writes a marker line, and a value you choose after that line stays).
EYE BLOOM COPY and SHARED LOD HIGHER also start on; their start-up verdicts are now judged only once the eyes
are known (as FAR SHADOWS), so they no longer revert themselves at start. tests/update372_tests.py fails if
any of them is off.

**Back to Update 36:** bloom_fx 0, wall_crawl 0, aim_steer 0, the health ring facing the eyes (ring_face 0).
Launcher defaults -BuildingReach 150 and -MonitorRate 30.

**Fixes.** Telemetry exports SpidyWorldTraceData, SpidyBloodData, SpidyPerfData, SpidyNpcFadeData,
SpidyColorData and SpidyFoveaData are all found by the launcher. Hand speed ignores timing glitches and
tracking jumps (run371 read 57,000 m/s and fired false hits). Sonic Burst needs both hands out at chest height
and has a 1 s cooldown. No gadget fires while a web is attached. Punch: a forward-ish hand at 2.4 m/s within
0.20 m (hooks and uppercuts count), so it lands more easily.

**New.** SLOT TEST (VR SETTINGS): each press tries the next input (RB+A, RB+B, RB+X, RB+Y, Y tap, Y hold,
RB hold, LB+RB, RB tap) 1.5 s later and logs it. WALL BODY (`wall_body`, default OFF). ZIP STRIKE
(`zip_strike`, default ON): a punch at an enemy 5-10 m away sends the game's web strike (Y; candidate).
WEB YANK (`web_yank`, default OFF; candidate input; never overrides a held stick). WEB BALL TEST (GRAPHICS):
EYE BLOOM OFF <-> COPY plus one web ball 1.5 s later, for the gold-squares A/B; the mip bias half needs
`-MipBias game` at launch. MIRROR ANCHOR (PLAYSPACE / WORLD).

**SM1 web feel.** About 0.15 s from palm to anchor, a wavy, erratic flight, a stiff snap taut on attach,
a thick white rope (web_width 1.8 cm) with a faint glowing jagged edge near you (far webs stay plain),
web_bend 100. **Web look: MOD.** The game's own web texture could not be confirmed in the exe, so the
mod's drawn web is used; no game-texture preview is given.

**COLOR / ATMOSPHERE** (hologram page 7, default OFF): see docs/color-atmosphere.md.
**NPC NEAR FADE** (VR SETTINGS, default OFF = people stay visible): see docs/npc-near-fade.md (all hook
addresses CANDIDATE, bytes verified before any write; NPC PUSH left out, candidates listed).
**FOVEATED (EXPERIMENTAL)** (GRAPHICS, default OFF): see docs/foveation.md.

## 48. Update 38: combat (punches, the zip, web-ball aim and look, hit registration, the ring, the hologram, wall tilt) and SpidyVR's combat logic

Every fix from the run372 diagnosis is a live setting on the hologram's VR SETTINGS page. Each one has a
one-sentence description, needs no restart, and starts at its recommended fix; the other values are plan B.
They are CRAWL CAP (AUTO / WITH FLAG / OFF), ZIP MODE (SPIDYVR / BOTH / DISTANCE / AIMED), BALL AIM
(SPIDYVR / CONE 20 / WIDE 35 / OFF; aim_steer stays 0), BALL LOOK (GLOB / GAME ONLY), PUNCH MODE
(BOTH / GESTURE / CONTACT, a 0.18 pulse, reject reasons logged), RING ANCHOR (HAND / WORLD, error logged),
HOLO OPEN (SAFE / LONG PRESS / F8 ONLY), WALL TILT (SAFE / OFF), WEB ROPE (ROPE / FIBRE), LUNGE (GAME BUTTON /
SPIDYVR MOVE, experimental) and BALL FIRE (PRESS / RELEASE). The full rules, addresses and performance
notes are in docs/combat38.md.

**Crawl cap.** The view is held at a crouch only under a ceiling below 1.6 m, seen 3 or more times over
0.3 s or more, and never within 1.5 s of combat input. A ceiling of 2.4 m or higher never does it.
Tested: a 3 m ceiling in a fight gives no cap; a 1.2 m duct gives a cap.

**SpidyVR's combat (logic only, no new addresses).** Body-relative fist speed, the re-arm rule
(below 1.2 m/s or 0.45 s), busy hands, a swept 8 cm fist against the enemy's capsule (2.2 m/s, inward),
the head-look lock (40 degrees, 10 m, 0.3 s), the lunge (2.6 m/s outward for 40 ms, 1.3-10 m,
37/60 degrees, in sight), SpidyVR's web-ball aim (about 7 degrees or the enemy's width, 45 m, wall
check), the yank (14 cm in 0.25 s at 1.3 m/s), and the web ball fired on the trigger's press. Combat
now gets a per-frame enemy list. Damage, fling and the game's shot hook are research only.

**Also.** Punches work while airborne or swinging. The zip covers 3-20 m, and enemies are scanned at
10 Hz. Web balls are a glowing white glob (WEB BALL TEST kept). NPC NEAR FADE logs enemies (its
addresses stay CANDIDATE). Wall tilt is reset on death or respawn and capped past 60 degrees off a
wall. The health log no longer ends a session on "no component". Sharpen 140 at start is now
applied (37.2 lost it). The 37.2 graphics defaults are unchanged.

**Protocol.** The swing Command is now version 6 (440 bytes, carrying up to 8 enemies and the lunge
mode); version 5 is still accepted. The swing Data stays version 4 (272 bytes). The new record
`SpidyCombat38Data` is version 2 (424 bytes), and `SpidyLungeSample` is new. The game DLL and the ray
module in this zip go together. With an older ray module, LUNGE stays GAME BUTTON.

**Tests.** spidy_tests (536), tests/update38_tests.py, and the 36 and 37.2 regression tests.

## 49. Update 38.1: the run38 diagnosis (aim axis, hologram, punches and lunge, ball lead, blood)

Every fix has its plan B as a live setting in the COMBAT section of the hologram's VR SETTINGS page, starting at the fix: AIM AXIS (HAND -Y /
AIM POSE / OLD -Z), RING LAYER (NEWEST / IMAGE), LUNGE FROM (2.5 / 1.3 / 4 m), LUNGE SPEED (2.2 / 2.6 /
1.9 m/s), LUNGE AIM (STEADY / 38), PUNCH ANGLE (WIDE .2 / 38 .35 / ANY 0), BALL LEAD (ON TARGET /
ALWAYS) and BLOOD LOOK (BIG / 37 SMALL); HOLO OPEN is TAP / PALM LOOK / CLICK / F8 ONLY. Details in
docs/combat38.md ("Update 38.1").

**Aim axis.** The fight now aims along webShooter(grip) -Z, the grip's -Y (the swing webs' axis);
38 used the grip's -Z, about 90 degrees off. The glob starts at the web shooter's nozzle.

**Hologram.** No hands-apart guard; a 7 cm zone over the wrist swallows the other hand's trigger and grip
(never a ball or gadget, and no Sonic Burst from it); TAP opens on a palm-up look plus a tap; F8 always.
The ring and hologram sit at the newest tracked hand and hide in a stale picture.

**Punches and lunge.** Only swings at enemies 2.5 m or further lunge; a punch needs forward cos 0.2; a
lunge needs 2.2 m/s and aim cos 0.7; the lock holds 0.5 s; the Y only at the locked enemy in sight;
rejected swings are counted once each.

**Blood** stays OFF by default; BIG is 1.1 s of bright 2-5 cm drops at the enemy plus a floor spatter.
**Gold web effect:** no switch (no confirmed runtime-only method); unequip the golden style in the
game's Gadgets menu.

**Protocol.** `SpidyCombat38Data` version 3 (512 bytes, `sequence` last). New exports SpidyAimAxis,
SpidyRingLayer, SpidyLungeFrom, SpidyLungeSpeed, SpidyLungeAim, SpidyPunchAngle, SpidyBallLead,
SpidyBloodLook (refusal codes 5312-5319). No new game addresses; the graphics defaults are 38's.

**Tests.** spidy_tests (541), tests/update381_tests.py, tests/update38_tests.py, and the 36 and 37.2
regression tests.

## 50. Update 38.2: the run381 diagnosis (holo, flicker, ring, kicks, gadgets, aim, grab web, blood, mirror)

Every fix is a live setting with its plan B. Five are new: MELEE ANIM (PUNCH ONLY / GAME), GADGET KEYS (38.2 / GAME
DEFAULT / CUSTOM), GRAB WEB (ENEMY / OFF), MIRROR LOOK (SUIT / FIGURE) and GADGET FEEL (EASY / 38.1). The
defaults are now SMART aim, TARGETED punches, WRIST ring, FOREARM holo, HOLD layer and BRUTAL blood. Root causes with file:line
references and the replay numbers are in the run381 diagnosis (DIAGNOSIS.md) and docs/combat38.md ("Update 38.2").

**Replay tests.** `spidy_replay_tests` (tests/replay_tests.cpp, ctest `replay`, gate G6) feeds the logged
poses and buttons of run381, run38, run372, run371 and run36 through the shipped classes. It covers holo opens, ring
hides, ring distance from the drawn wrist, X with no enemy and with one, gadget recognition, aim, grab web
and yellow squares in our marks. Enemies are synthesised.

**Gold squares.** These are the game's golden style, not our drawing: Pause > Gadgets > select Web Shooters > highlight the LAST upgrade (Golden) > toggle it off; repeat for each gadget.

**Protocol.** `SpidyCombat38Data` version 4 (664 bytes); exports SpidyMeleeAnim, SpidyGadgetKeys,
SpidyGrabWeb, SpidyMirrorLook, SpidyGadgetFeel (refusal codes 5320-5324). No new game addresses; graphics
defaults unchanged. The new DLL needs one game restart; the switches are live after that.

## 51. Update 38.3: the run382 diagnosis (gadgets, double web ball, look lock, grab and yank, night windows, mirror suit)

Every fix is a live setting with its plan B. Four are new: LOOK LOCK (QUICK / STEADY), STRIKE STEER (TO LOCK / OFF), FAR
WINDOWS (2X / 3X / 1.5X / GAME) and GADGET HINT (ON / OFF). The new defaults are GAME ONLY ball look (our glob is off),
GRAB WEB ASSIST and WEB YANK on. A 38.2 live.txt is moved to those three once (migrate_383). Root causes with file:line
references and the replay numbers are in the run382 diagnosis (DIAGNOSIS.md) and docs/combat38.md ("Update 38.3").

**Replay tests.** `spidy_replay_tests` also replays run382, and adds [9] look lock (deliberate looks followed 16/16,
was 2/28; 0 stray switches, was 17) and [10] grab assist (408/408 within 12 degrees, was 194/408; 0 false past 13 degrees).

**Mirror.** The SUIT figure is plan B: a Spider-Man shape on the solved body pose, in colours learned from the eye
images. No suit ID is read, because no SM2 address for one is confirmed.

**Protocol.** `SpidyCombat38Data` version 5 (768 bytes); exports SpidyLookLock, SpidyStrikeSteer,
SpidyFarWindowsOption, SpidyGadgetHint (refusal codes 5325-5328). FAR WINDOWS: one CONFIRMED write (SHARED LOD reach) and one
CANDIDATE write gated on three existing byte checks. No new game addresses; graphics defaults unchanged. The new DLL
needs one game restart; the switches are live after that.

## 52. Update 38.4: the MIRROR PROBE (what a game-model mirror needs)

**MIRROR PROBE.** `include/spidy/mirror_probe.hpp` and `src/mirror_probe.cpp` are in `spidy_stereo_probe` (the VR module).
They are fed by four existing stereo_probe hooks: start, the end of maintain, copyJob, and renderActor before the hero
hiding (1 frame in 32, at most 4096 draws). The probe is read-only: SEH-guarded copies only, no writes, no game calls,
no new hooks. It is on by default with no setting. It logs slot use and view-list headroom (cap 8 at 0x2e15631), the
hero and his pieces per view kind, hiding by the port vs the game, the CANDIDATE camera switch byte 0xc417360, and the
suit-field tally on model changes. Its cost is self-measured; it stops above a 60 us mean or on a fault.

**Protocol.** `SpidyMirrorProbeData` (MPRB, version 1, 1624 bytes) is `update37_records.RECORDS['mirror_probe']`, in
the game-vr report and the console. The research and how to read the record are in docs/mirror.md.

**Tests.** `spidy_mirror_probe_tests` (ctest `mirror_probe`) covers the view-list rule, the view kinds, the root-transform
match, the suit tally, the cost guard, and a worst-case frame under half the budget; `--dump` writes a record for
`tests/update384_tests.py`. That file checks the parser against the native layout, the read-only scan, the hook lines,
no GAME MODEL setting, and the defaults.

**No GAME MODEL switch.** Nothing confirms it would work (docs/mirror.md section 10). No new game addresses are
written; graphics defaults are unchanged. The new DLL needs one game restart.

## 53. Update 38.5: BALL LOOK WRIST, one ball a press, far punches, PROBE v2, the HERO SWITCH PROBE

**BALL LOOK WRIST** (the new default; values WRIST 0, GLOB 1, GAME ONLY 2). `include/spidy/wrist_muzzle.hpp` and
`src/wrist_muzzle.cpp` are in `spidy_stereo_probe`. They hook the web shooter's muzzle functions (CANDIDATE 0x373e1b0 /
0x373e260, vtable 0x53c1d98). Their first 16 bytes are checked first, and nothing is hooked on a mismatch. Only the web
shooter's calls within 0.30 s of the port's own ball press, with a shift of 2.5 m or less, move row 3 to the pulled hand's
wrist (motion.grips -> webShooter, world space). The record is `SpidyWristMuzzleData` (WMUZ v1, 144 bytes).
`live_settings.migrate_385` moves a saved GAME ONLY to WRIST once.

**One ball a press** (`combat_input.hpp`): each press queues its own RB pulse, `ballGap` apart; `Out::ballStart` names the hand.
**Far punches** (`CombatConfig::farLunge`, set by game_xr): a 20 m lock and lunge reach, a wider aim cone (`reachAim`), a time
re-arm and `farFree`.

**PROBE v2**: `SpidyMirrorProbeData` MPRB v2, 2752 bytes; the v1 fields are in place (docs/mirror.md section 11).
**HERO SWITCH PROBE** (`include/spidy/hero_switch.hpp`, fed in game_xr's frame loop, no setting) is read-only.
`SpidyHeroSwitchData` is HSWP v1, 112 bytes, `update37_records.RECORDS['hero_switch']`, with one console line per hero replacement.

**EXTRAS** (`include/spidy/extras385.hpp`, holo page 8, `Control::invincible` ... `playOnMonitor`, exports `SpidyInvincible`,
`SpidyFlyMode`, `SpidyFlyVignette`, `SpidySymbioteLook`, `SpidyPromptHandling`; refusal codes 5344-5348). INVINCIBLE writes the
hero health record's +0xd0 only under vtable 0x541edd0 (CONFIRMED) via `invincible::refill` (never above +0xa0, never at 0). FLY
drives `native_movement` (CONFIRMED) while airborne; `fly::Flight` is pure and tested; its punch lunge reuses the lock reach and
sends no Y. SYMBIOTE LOOK is overlay only (`symbiote::appendGlove`, web style, mirror palette); `symbiote::ChestPull` toggles it.
`world_trace::checkpointBit` (4096) marks classes named for a checkpoint (CANDIDATE, read-only).

**GESTURE MOVES** (`include/spidy/gestures385.hpp`, `Control::gestureMoves`, `SpidyGestureMoves` 0-2, refusal 5349, live.txt
`gesture_moves`, kept from live.txt). game_xr runs `Recognizer::update` once a frame beside the chest pull (airborne = last
frame's `inFlight`, the hologram and the physical stick block it). TRAINING sets `trainingFails_` and only prints `NAME - OK` /
`NAME - FAIL: reason` (a FAIL is said at most once per 0.6 s a move). ON queues each successful non-gadget move's combo in
`ComboSender` (lead 50 ms, then lead|face, up to 4 queued), ORed into the pad just before `game_pad::submit`; the wings' stick goes out
only while the stick is under 8000. `gestureSet.wingArms` is off while FLY MODE is on. The pad bits are static_asserted against
`game_pad::Button`.

**GAME HUD research** (`tools/vr_display.py hud_option_probe`, launcher `game_hud_options`): read-only walk of
HKCU\Software\Insomniac Games\Marvel's Spider-Man 2 for UIHideHUDEnabled and 7 related names. The exe's registry name table holds
only window/graphics names (and ScreenShake), so the values most likely live in the profile; nothing is ever written.

**Holo subtext wrap and scroll** (`wrist_hud::helpInfo`, `helpOverflow`, `helpAt`, `helpLineWidth`, `helpLines`). Every help
text is wrapped at `helpWidth - helpGutter` using `helpLineWidth` (the larger of the vector width and the pixel font's columns), and
`wrapText` splits a word longer than the line. Each item keeps `helpRowsShown` (3) rows; a longer text is drawn sliding through them
by `View::helpScroll` lines and clipped to them with `clipBand`, with a dimmed edge row, arrows and a bar in the gutter. `Menu::update`
owns the scroll: the thumbstick scrolls the highlighted help first (`helpStickLines` a second) and passes to the page at its ends, a
press on the help rows starts `Drag::help`, and after `helpAutoDelay` (1.5 s) of hover it steps a line each 0.5 s, holds 1.2 s and
glides back (`helpBackLines`). Manual input stops the auto-scroll for that control. game_xr sends the drawn overflow back through
`wristHelpOverflow` (`Input::helpControl`, `helpOverflow`), and `HoloKey::help` keys the holo cache so a scroll redraws. core_tests
"SUBTEXT GATE" and "SUBTEXT SCROLL"; preview sheet `spidy_hud_preview <bmp> subtext`.

**Performance pass (38.5).** Everything new is off by default and costs nothing when off. GESTURE MOVES' recognizer is
allocation-free: 0.55 us a frame on this box (-O2, 900,000 frames of moving hands). BLOOD EXTREME is capped at 880 quads
(`budgetQuads`) for every splash, coat and visor drop at once. The 90s ANIMATED toon replaces the sharpen pass's pipeline (one
full-screen pass, no extra one). FLY and INVINCIBLE do a few float operations a frame.

**GESTURE REMAP (38.5).** `gesture_remap.hpp` holds the matcher: `Capture` turns head, look and grip frames into body-relative,
arm-normalised motions (start speed and travel thresholds, so a hologram tap is no motion); `Learner` runs the three takes (RED,
BLUE, GREEN), the per-take reasons and the conflict check against `references()` (the built-ins), the player's other moves and
`gesture_everyday.hpp` (generated by `tools/gesture_everyday_gen.cpp`); `Detector` plays saved moves; `Store` keeps four profiles
and round-trips through `save`/`load` (versioned text, unknown lines ignored). `gesture_session.hpp` is the hologram's side: the
presses, the `View` the EXTRAS trail draws, and `inputOf` (gadgets as RB combos). game_xr loads `%APPDATA%\Spidy\gestures.txt` in
a try/catch, feeds a frame each VR frame (recording runs with the hologram open; playing needs GESTURE MOVES ON and the hologram,
phone and pad layer down), skips a built-in event for a move with a custom motion (and every built-in while recording), sends a
custom move through the `ComboSender`, and saves through `writeWhole` (`.new` then `MoveFileExW`). A throw sets `remapFault` and the
built-ins carry on. Preview: `spidy_hud_preview <bmp> remap`.

**Release gate (38.5).** `tests/release385_tests.py` maps blockers 1-8 to the core and replay test names that must PASS (from
ctest's LastTest.log, or by running the built tests), and checks statically: experimental atomics start OFF, REALISTIC FLIPS off,
GESTURE MOVES and REMAP each turn themselves off on a fault, the hero switch probe has no write, PAD MODE is on the hologram.

**Tests.** `tests/update385_tests.py` covers the migration, the subtext, the WMUZ/HSWP records and the parser, and the read-only
probe. In core_tests, "HERO SWITCH PROBE" covers the switch timing, and the BALL LOOK defaults are covered too.
`spidy_replay_tests` covers run384 (far punches, balls per press, ball hand, ball origin). core_tests' "EXTRAS", "SYMBIOTE LOOK",
"FLY MODE's punch", "GESTURE MOVES", "GESTURE REMAP" and "GESTURE REMAP on the hologram" cover the extras; update385_tests covers their settings, the help-line rule and the HUD probe.

## 54. Version 0.0.1 (build 38.5): FLY poses, R3 drop, FLY ROLL, and the public version

**FLY poses** (`extras385::fly::Poses`, live only with FLY MODE on or GESTURE TRAINING, in play, X not held, no hologram, pad
layer or phone; game_xr builds `PoseInput` from the grips and head next to SYMBIOTE LOOK's chest pull). Body frame: x right, y up
from the eyes, z forward. RISE: one hand at least 0.12 m above the eyes (exit 0), lift scaled by height. FORWARD: both hands
0.42 m ahead (exit 0.32), between 0.70 m below and 0.30 m above the eyes, at most 0.75 m apart. STOP (T-pose): hands at least 1.0 m
apart (exit 0.85), level with the shoulders, within 0.40 m front/back. Holds 0.15 / 0.25 / 0.15 s, leave 0.15 s, and a hand over
1.6 m/s resets the FORWARD hold, so a punch is never a pose and fly lunges still work. Anything else is HOVER (arms down hovers,
never a drop). The pose, its lift (0-15) and the drop bit ride in `reserved6` bits 25-31 (`fly::pack`); the swing module's
`fly::Flight` ranks THRUST > DROP > STOP (brake 2.5) > RISE (top x 0.6 x lift) > FORWARD (top x 0.85) > STICK > HOVER and lands when
grounded with target y <= 0.5. **R3 / L3+R3** (`fly::StickChord`, only while flying): R3 alone latches a drop (game_xr holds it until
landed, the RISE pose, or 15 s); L3 + R3 within 0.12 s and let go before `pauseHoldSeconds` taps the game's L3 + R3 for 0.1 s
(the ultimate). While flying R3 is not the web pull and the raw thumbs are not sent. **FLY ROLL** (`fly::Roll`, settings 5350
mode, 5351 speed 30-180, 5352 auto level; hologram EXTRAS rows FLY ROLL / ROLL SPEED / AUTO LEVEL): the right stick's x rolls
`GameTrackingRig::flyRoll` (the tracking space tilted about the head's level look, after the WALL ROLL block, the head kept in
place), and the rig's stick turn is off that frame. It is a MOD-SIDE view transform, not the game camera; the hero is not rolled.
COMFORT clamps to 45 degrees and draws the FLY vignette at up to 0.6 plus a horizon bar 1.5 m ahead, rolled back to the game's
level. Faults: poses or roll throwing turn FLY MODE / FLY ROLL off with a note, never a crash or flat screen. Remap: four FLY
references (RISE, FORWARD, STOP, HOVER; `Kind::builtin`, any buttons) flag a recorded motion that looks like a pose. R3 is a
button, so it cannot be part of a recorded motion.

**Public version.** `project(Spidy VERSION 0.0.1)` (was 0.4.1) is the one source: `SPIDY_PUBLIC_VERSION` for code
(`include/spidy/version.hpp`), the module and launcher resources, `VERSION` for the tools and the player's folder. PORT-VERSION stays
"Update 38.5" (gates G4 and the update tests read it). Shown top right on the hologram's STATUS page ("VERSION 0.0.1"), run_game_vr's first log line, the
reports' `version`, the address finder, launch/build/collect-reports windows and the guide's title. tools/release.ps1 and
release.yml raise CMakeLists.txt and VERSION together; tests/version_tests.py checks they agree. Rule: docs/VERSIONING.md.

## 55. Version 0.0.73: IMAGE (SPIDYVR, the default, or HERO VR CLASSIC)

After test 3 the owner reported that streetlights still flicker (even with LIGHT HOLD), the shadows are good, and the aliasing is too
noticeable while the smoothing looks smudged and blurry. He asked that it look as good as SpidyVR does. **SpidyVR (upstream 0.6.0)**
runs the game's own TAA per eye, each eye with its own history, Halton jitter and a 67% render scale. It sets the game's
Sharpness to its lowest, turns DoF off and uses an anisotropic 8x copy. It has no FXAA, no CAS and no mip bias of its own (stereo_probe.cpp,
game_xr.cpp, eye_resolution.hpp, launcher_text.hpp, tools/vr_display.py). HERO VR's ENGINE TAA (opt-in) still shakes distant
buildings (CANDIDATE: its jitter units and per-eye motion vectors), so it cannot be the default.

**Root causes in test 3's image.** These are CONFIRMED from the 0.0.73 export:
- EYE AA read its history bilinearly while the head moved in 32577 of 32652 pairs and kept 80% of it, so the image smudged.
- FXAA (aa 1) smoothed on top of that.
- A +0.5 texture mip bias replaced the game's −1.0.
- Sharpen 140 (past CAS's top) brought back ringing and shimmer.

The streetlight flicker is CANDIDATE: the history was cut in 26% of pairs, and sharpening and EYE BLOOM COPY amplify aliased bulbs.

**IMAGE** (combat38 `image`, wrist GRAPHICS > QUALITY, live.txt `image`, export SpidyImage 5449):
- **SPIDYVR** (0, the default): aa 0, sharpen 0, the history read Catmull-Rom (native_eye_gpu.cpp `sharpOld`, SpidyEyeHistorySharp),
  and texture mip bias 0 from the next start when the launcher passes its old default 0.5.
- **HERO VR CLASSIC** (1): test 3's look (aa 1, sharpen 140, bilinear read, mip bias 0.5).

Under both, EYE AA, history 80, LIGHT HOLD, SHADOW MATCH, CULL MARGIN, FAR SHADOWS and EYE BLOOM are unchanged. The
history read, FXAA and sharpening change live; the textures change on restart. `migrate_073t4` (marker MIGRATED_073T4) moves a
live.txt's test 2/3 values (aa 1, sharpen 140) to 0 once, unless it picks `image = classic`. STARTUP_ON no longer forces sharpen 140.

**DD BATON TIP** (combat38 `ddBatonTip`, wrist DAREDEVIL, live.txt `dd_baton_tip`, export SpidyDdBatonTip 5453; ON by default, live):
- **ON** (0): the held baton is seated down the fist's own tube through the grip (`dd_baton073::seated`; 9.5 cm of butt
  under the fist, the rest past the thumb), and the suit's fist is fitted round it (`suit_wear` Tracked/Skeleton `fistFit`:
  per suit, `makeRig` fits the rest fingers' droop line and searches the hole the curled fist leaves; only the held hand,
  only while held). The grapple's start (ddAimFrom) is the silver tip on its axis. While that hand's grapple pulls, the
  tip (the model cut at its seam, `batonPart`, nothing flattened; the club's steel tip as fallback) flies to
  `capeNow.anchor` in 0.12 s on a grey rope from the seam (`dd_baton073::TipFlight`/`place`), stays there, and reels
  back in 0.25 s and snaps on when the pull ends; the hook line is not drawn for that hand meanwhile.
- **OFF** (1): test 3's shaft (`baton_grip::shaft`) and fist exactly. The trigger-grapples / grip-release-throws
  behaviour (c40b091) is the same in both.
- CONFIRMED: curl 1 while a baton is held (game_xr), Touch tilt 30 deg. CANDIDATE: Index 15 deg, the Steam Frame as
  Touch, the fist-hole fit parameters (a 36 mm handle scaled by the rig's height), `capeNow.anchor` as where the tip
  lands. No new game addresses.

## 56. Version 0.0.73 test 5: HERO HANDS (Iron Man), GRAPPLE AIM (Daredevil), CLAW LOOK (Wolverine)

After test 4 the owner reported: "whatever you did to fix ironmans hands did not work. i feel like you are applying one fix to
every heros hand but every hero works differently", that the raised Iron Man hand must be "palm faced forward and only on iron
man", that Daredevil's silver tip stands straight up while grappling, and that Wolverine's claws' "fan out is fine but
placement, color and rotation are totally screwed". Each fix is ON by default with a wrist switch to turn it off, all live
(no restart). No new game addresses.

**HERO HANDS** (combat38 `ironHands`, wrist `HERO HANDS`, live.txt `iron_hands`, export SpidyIronHands 5462):
- Root cause: every suit went down one hand path (`restHands`' votes for the back of the rest hand). On the gauntlets that
  vote is wrong: `tools/hero_hands073/palm_disc.py` measures MARK 85 33, MARK 7 49 and MARK 6 17 degrees off, so the
  repulsor disc was never the real palm and test 4's shared fix moved every hero's hands alike.
- ON (0): `suit_wear::HandProfile`, a per-suit profile (`handProfile(folder)`, applied at `makeRig`). IRON MAN's MARKs:
  rest turned 35 / 0 / 50 / -18 deg (Suit_ironMan_1..4), forearm follows 0.75 of the roll, the trigger opens the hand,
  and a hand raised palm-forward (`raisedPalm`: the palm within about 55 deg of the level look ahead, the grip no lower than
  40 cm under the eyes) is drawn palm out along the blast, eased in and out. Only Iron Man has `palmRaise`/`triggerOpens`.
- OFF (1): the shared profile for every suit (test 4's hands exactly).
- CANDIDATE: every turn value (measured from the models, not on a headset), the 55 deg / 40 cm raise gate.

**GRAPPLE AIM** (combat38 `ddGrappleAim`, wrist DAREDEVIL `GRAPPLE AIM`, live.txt `dd_grapple_aim`, export SpidyDdGrappleAim 5463):
- ON (0): while that hand's grapple aims (trigger at least `aimTouch` 0.1), fires or reels, the seated baton turns about the
  grip point (`dd_baton073::aimTurn`) until the silver tip points along the controller's aim (eased in 0.12 s, back to the
  fighting grip in 0.25 s). The ray leaves the turned tip along the aim (`apps/dd_baton_preview` scene `aim`: 0.0 deg off).
- OFF (1): the fighting grip (test 4's DD BATON TIP seat).
- AIM HANDLE (after the owner's "silver part is straight up" report the turned handle clipped out of the bottom of the fist):
  the turn's pivot slides `aimPivot` 5 cm up the seated handle (the fist's top, by the thumb and index) and the baton slides
  through it until only `aimHold` 1.2 cm of its butt is behind that pivot, so the handle leaves the front of the fist instead
  of crossing it. `apps/dd_baton_preview` scene `aim` poke counts (fist points through the handle, left/right), before -> after:
  suit 1 70/62 -> 6/5, suit 3 169/144 -> 6/22; level aim suit 1 127/129 -> 8/13, suit 3 141/141 -> 15/15; aim across
  suit 1 161/111 -> 8/8, suit 3 269/70 -> 17/0. The fighting grip is unchanged (suit 1 23/15, suit 3 7/7).
- CANDIDATE: `aimTouch`, the timings, `aimPivot` and `aimHold` (swept on the preview, not on a headset). GRAPPLE AIM OFF is the
  fallback.

**CLAW LOOK** (combat38 `wolverineClaws`, wrist WOLVERINE `CLAW LOOK`, live.txt `wolverine_claws`, export SpidyWolverineClaws 5464):
- Root cause: the claws model (Models/Claws, base at the origin, -z along the claws, x the knuckle row, +y the back of the
  hand) was placed x -> the grip frame's right and y -> its up (toward the thumb), so the claws came out of the thumb side,
  fanned across the wrong axis and floated ahead of the fist; its texture reads rusty in game.
- ON (0): `hero007::clawSteelFrame` / `appendClawSteel`: y the back of the hand (right +X, left -X), x the knuckle row,
  -z along the punch, the base `clawSteelAhead` 5.5 cm ahead of the grip and `clawSteelBack` 2.4 cm toward the back of
  the hand; drawn untextured, shaded bright steel (diffuse, specular, rim). Retracting slides them into the knuckles as before.
  WOLVERINE's own hand profiles (`wolverine_classic` +50, `wolverine_jacket` +20, `wolverine_weapon_x` 0) tighten the fist
  round the claws; HERO HANDS on Wolverine follows CLAW LOOK.
- OFF (1): test 4's claws and the shared hands.
- Fix 10 is unchanged: every Wolverine suit uses the JACKET AND JEANS claws, retractable, never doubled.
- CANDIDATE: both offsets, the three turns (the most compact fist in `apps/hero_hands_preview`). Limits: CLASSIC's rest hand is
  truly low-poly (about 280 triangles a hand), which a profile cannot smooth; WEAPON X's own suit mesh has knuckle spikes.

**CLAW BONE** (CLAW LOOK ON with a Wolverine suit worn; the owner: "none of the wolverine claw things actually looks like it
comes out of the hand looks like the all float around it"):
- Root cause: the claws were placed by the controller grip (`gun_grip::held`), not by the suit's drawn hand, so on each suit
  they started where that suit's fist is not: `apps/hero_hands_preview HANDS_CLAWS=steel` measures every claw base OUTSIDE the
  drawn hand (winding 0.003-0.015), 13-17 mm from the nearest hand vertex on CLASSIC and 46-53 mm on JACKET and WEAPON X.
- ON: `suit_wear::findClawRoots` (at `makeRig` / `applyHandProfile`, Wolverine suits only: about 215 ms once at model load for
  the three) finds, in the rest hand, three roots between index/middle, middle/ring and ring/pinky, under the back of the hand
  at least `clawDepthMin` under the skin, `clawBehind` 2 cm behind the knuckle line, and the way out along the hand, tilted
  between `clawTiltDeg` 8 and `clawTiltMaxDeg` 30 deg so each blade leaves the skin `clawExitDrop` under the top of the knuckle
  row. Each frame (`game_xr`, after the suit is posed) `suit_wear::clawPlace` carries them by the hand bone's own move and
  `hero007::appendClawBone` sets each of the model's three claws rigidly on its root, fanned as made; retracting slides each
  back along its own line into the hand (what passes the root is kept at the root). When the suit cannot be posed this frame,
  or a hand has no roots, the claws are drawn by the grip (`appendClawSteel`) - never missing. The mirror keeps the grip's claws.
- Measured (`HANDS_CLAWS=bone`, fist, fully out; 64-ray "hidden" test: a point is hidden when 58 of 64 rays meet the skin
  within 25 cm): every root HIDDEN and every root end's corners hidden 36/36 on all three suits, both hands. CLASSIC: roots
  14-20 mm deep, the blade out of the skin 23-41.5 mm along it (+0.8 to +17.6 mm past the knuckle line: its curled fingers
  cover the line); JACKET: 1.5-4.6 mm deep, out at 18.5-19.5 mm (-1.4 to -0.3 mm); WEAPON X: out at 23.5-29 mm (+3.4 to
  +8.5 mm; its curled fingertips pass into its palm at the roots, so one root's winding reads 0.006 while the root is hidden).
  Twist and raised grips and half out (`HANDS_EXT=.5`): the same, all hidden.
- CANDIDATE: every constant above (from the three models, not on a headset). OFF (CLAW LOOK 1): test 4's claws, as before.

Previews: `apps/hero_hands_preview` (`HANDS_CLAWS=steel|old|bone`, `HANDS_EXT`), `tools/hero_hands073/render_hands.py` (`--near=`),
`tools/hero_hands073/render_claws.py`, `apps/dd_baton_preview` scene `aim` (`DD_AIM="x y z"`, `DD_AIM_PIVOT`, `DD_AIM_HOLD`).

## 57. Version 0.0.73 AA: SHARPEN MATCH, GAME TAA texture detail, the console jitter label

Owner after test 5: "way too sharp and jagged when i move, too blurry when im still". Full analysis: `docs/AA-0.0.73.md`.
- CONFIRMED cause (test 5 logs): IMAGE stayed CLASSIC (FXAA + eye history 75 + CAS 100). GAME TAA ran ~35 s, then was pressed
  OFF. While moving, the history was cut whole in 17090 of 35182 frame pairs, so those frames were raw render + FXAA + CAS 100
  (jagged, over-sharp). When still, 75% of a rotation-only bilinear history was kept and mip +0.5 replaced the game's negative
  biases on 266 samplers (blurry).
- SHARPEN MATCH (DISPLAY AND DETAIL, `sharpen_match`, export `SpidySharpenMatch`, 5473; ON default, live): CAS is scaled by
  the history weight actually kept this frame (`eye_sharpen::matchedPercent`), so no sharpening while you move. OFF = test 5.
- GAME TAA SPIDYVR + engine TAA: mip bias `0.5` becomes `0` (`live_settings.image_mip_bias`), so textures keep their detail
  under the game's TAA; restart. `-MipBias` still overrides it.
- `engineTaaPublish` takes its jitter mode from `image_style::jitterMode`, so the console line names the jitter really used.
- CONFIRMED: the jitter units equal the game's (Lens 0x2c75560 stores +0x428 = 2·argX, +0x42c = -2·argY). No new addresses.
- CANDIDATE: the DLAA path (game AA 5-7) per eye, mip 0 versus `game` at 100% eyes; Windows build not compiled here.
