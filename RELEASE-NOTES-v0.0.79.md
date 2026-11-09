# HERO VR 0.0.79 — Revamped Performance Update

**EXPERIMENTAL — VR renderer improvements.** The most recent published version before this is 0.0.78. The older 0.0.63 remains the previous stable reference. Spider-Man 2 2.810 is required.

## What's new

- **Rendering choice on the launcher's first screen:** Original or Revamped. Both modules ship in the download; switching selects the proper module at the next VR launch, not while the game is running. A short description updates when the player toggles.
- **REVAMPED (default):** optimized 70%-resolution per-eye rendering reconstructed to full resolution with a temporal anti-aliasing/sharpening pass. The tester reported approximately 59 FPS while standing/walking and 52 FPS during fast movement on their system, with substantially less image swimming. FPS depends on hardware and scene.
- **ORIGINAL:** the unmodified official 0.0.78 stereo renderer, with the standard engine-AA settings, remains available as a direct fallback.
- **Special Thanks:** Venom Cash, Gogeta, and Tim The KoolAid Lover are credited for testing alongside existing contributors.
- **No game reinstall needed to change renderers.** Exit the game, choose your rendering mode in the launcher, and click PLAY VR.

## Known issues and next priorities

- Windows on buildings may suddenly illuminate or switch off as the camera moves. Attempts to force lit buildings to greater distances increased flicker and **are intentionally not included** in this release.
- Building textures may sharpen late and diagonal building lines may show aliasing; Revamped reduces but does not eliminate the problem.
- Existing 0.0.78 story-mode issues are still being investigated (including Sandman progression, some objectives and chase scenes).

## Install and update

- **New installation:** Download `HERO-VR-Launcher-0.0.79.zip`, extract the whole archive, and double-click `HERO VR Launcher.cmd`.
- **Existing installation:** Use the launcher's Check for Update flow. The release manifest includes the complete source/update ZIP and matching prebuilt native modules.
- Change the mode before starting VR. Original is an instant fallback if Revamped behaves poorly on a particular GPU.
- Source and licenses are distributed under the project's GPL-3.0 terms.
