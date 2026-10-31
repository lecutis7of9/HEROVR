# 0.0.74 test 3: two-GPU device lost (0x887A002B, eye bridge 3301)

Made by Agent Mulder.

Sessions: public 0.0.73 player, `game-vr-20261029-204133` (1c) and `game-vr-20261029-225454` (1d).
Every finding is marked **CONFIRMED** (read in the logs or the code) or **CANDIDATE** (an inference not yet proved).

## What the logs show

- **CONFIRMED** The PC has two graphics chips. The game lists four adapters: NVIDIA GeForce RTX 5090 (32 GB, device 2b85), AMD Radeon(TM) Graphics (device 13c0, a Ryzen processor's own graphics, 485 MB), Microsoft Basic Render Driver, and the RTX 5090 again.
- **CONFIRMED** Both sessions die the same way, about 4 s after the save loads and after "VR active" (the eyes had made 32 renders):
  1. `CreateHeap ... failed with HRESULT 0x887a0005`, then
  2. `Device removed detected (0x887A002B)`, which is DXGI_ERROR_ACCESS_DENIED as the removal reason, then
  3. the game's display-mode validation fails ("No valid display mode could be found for the desktop").
- **CONFIRMED** The eye bridge's 3301 is a consequence, not a cause. It is `fence->GetCompletedValue() == UINT64_MAX` (src/native_eye_gpu.cpp, `readyCommands` and `capturePair`), which D3D12 returns once the device is already removed. The game's CreateHeap failure (0x887A0005, DEVICE_REMOVED) is also a symptom: it found the device already gone.
- **CONFIRMED** Only 1c has the game's `[Mem12] Aliased resource does not fit in the original heap ... Recreating heap and aliases.` line (4 s before the loss). 1d has no such line and dies the same way.
- **CONFIRMED** The player had STREAMER MODE, SL WATCH and LIGHT CHECKS all on (the defaults). Reflex, frame generation and the RT effects were already off for VR. The runtime was SteamVR (Quest).

## Causes, ranked (all CANDIDATE)

1. **The two-adapter display path.** The monitor (or SteamVR's compositor) is on the Radeon while the game renders on the RTX. A resource shared across the two (SteamVR's swapchain images, or the game window's present) is opened by a device that may not access it, which gives ACCESS_DENIED. This is the only thing that sets this PC apart from players with the same defaults and no loss. The failed desktop-mode validation right after the loss points at the display side. Partly covered by the 0.0.74 TWO-GPU CHECK (Windows' High performance preference for the game, vrserver and vrcompositor) and by the "monitor into the graphics card" step.
2. **STREAMER MODE.** Every immersive frame, it copies the shown eye (a SteamVR swapchain texture, a shared resource) into the game window's image, which the monitor then presents. That makes STREAMER MODE the mod's own per-frame cross-device access, and it starts exactly at "VR active".
3. **LIGHT CHECKS.** It widens the eye views' query pool from 256 to the game view's size (at least 1024) through the clamp byte in 0x2c98350. The patch site is CONFIRMED in stereo_probe.cpp; that it could cause the loss is CANDIDATE. If the game's query heap behind an eye were smaller, the GPU would write out of range at the first eye frames. It is weak: it has been on for every 0.0.73 player, and out-of-range writes usually report HUNG or REMOVED, not ACCESS_DENIED.
4. **SL WATCH.** A CPU-side log of the game's Streamline calls (slSetConstants, slEvaluateFeature, tags). It adds no GPU work, so it is the weakest of the three. It is a developer test with no value to players.
5. **The game's render-heap re-creation (Mem12)** while the eye bridge holds the aliased eye images. This is seen in 1c only.

## Does HEAP GUARD cover them?

**0.0.74 test 10:** HEAP GUARD is removed with the other 0.0.74 picture options (the eye bridge is v0.0.73's again), so this section is history. The SAFETY CHECKS switches and the pop-up below stay.

- **Cause 5:** yes. With HEAP GUARD on (ON by default in fa15f5f; OFF by default since the test 3 black-headset fix, docs/BLACK-074-test3.md), after the game makes a new render heap the bridge lets go of the eye images, forgets their states, takes them again from the next frame, and copies no pair begun before the heap change.
- **Causes 1 to 4:** no. HEAP GUARD does not touch the display path, the streamer copy, the query pools or Streamline. 1d, with no Mem12 line, would most likely still fail with HEAP GUARD alone. The player ran 0.0.73, which had no HEAP GUARD, so it was not yet tested on this PC.

## What test 3 does

- **Pop-up (launcher).** It appears when a session's console shows `0x887A002B` or `GPU eye bridge: 3301`, the session was not stopped by the player, and CLEARER ERRORS is on. Its Yes button writes `streamer_mode = off`, `sl_watch = off` and `light_checks = off` into the mod folder's `live.txt`, then says to press START VR (PLAY VR). With CLEARER ERRORS off, the old failure window shows instead. Code: MainForm.cs `OfferTwoGpuFix`, LiveFile.cs.
- **Switches.** OPTIONS > SAFETY CHECKS has STREAMER MODE, SL WATCH and LIGHT CHECKS. They read and write `live.txt` itself, not a launch argument. Each has a one-sentence note on whether it needs a restart: STREAMER MODE and SL WATCH change live, and LIGHT CHECKS takes effect at the next START VR.
- **Console.** The CLEARER ERRORS steps for 0x887A002B add step 5, which turns the three off.
- **Defaults.** None changed: all three stay on, because nothing yet shows which one causes the loss.
- **Proposed, not applied: SL WATCH default off.** It is a developer diagnostic, it never helps a player's picture, and it is one less hook in every gameplay frame. Apply it once the owner agrees.
- **Next evidence to collect.** A run on this PC with the pop-up's fix applied (all three off) and HEAP GUARD on, then each one turned back on in turn. That is what moves any of causes 2 to 4 from CANDIDATE to CONFIRMED or ruled out.

## 0.0.74 test 10: the monitor's copy is always on (no switch)

- The owner chose that the PC monitor always shows the headset's left eye with the mod's drawings. 0.0.73's STREAMER MODE copy path is back, always on, with no wrist row, no `live.txt` key and no launcher switch. Old `streamer_mode`, `streamer_smooth` and `streamer_eye` lines are still skipped quietly (RETIRED_073), and the pop-up turns only LIGHT CHECKS off.
- The copy goes into the mod's own texture on the game's device. The window gets it only if the window's image belongs to the game's device and the window's monitor hangs off the same GPU (DXGI output's adapter LUID == the device's LUID, checked once a session). Otherwise there is no copy on the window (cause 2's cross-GPU path is avoided).
- On any problem the window keeps the game's own view (0.0.72) for the rest of the session: the window on another GPU, a float back buffer, the shader not building, the copy not possible, GetDeviceRemovedReason failing, the eye bridge's fault or lost eyes. The headset is never touched by this. One console line says which picture the monitor shows (`MONITOR: ...`).
- Still a risk: a device removal ends that session anyway, and the next session tries the copy again. If cause 2 is the real one on a PC whose monitor is on the game's GPU, the pop-up can no longer turn it off.
- **Remembered across sessions (MONITOR MEMORY, no setting).** `%APPDATA%\Spidy\monitor-copy.txt` says `copy` while the copy is on (removed on a clean exit) and `lost` after a real device-removed sign (GetDeviceRemovedReason failing, or the fence's UINT64_MAX, bridge 3301). It becomes `skip` (the copy is skipped from then on, the monitor shows the 0.0.72 game view, and the MONITOR line says why) when the next session finds `lost`, when the launcher sees 0x887A002B / bridge 3301 in a session's console while the file said `copy` or `lost`, or when the player presses Yes on the launcher's device-lost pop-up. A session merely killed or crashed for another reason leaves `copy`, which changes nothing. Deleting the file tries the copy again.
