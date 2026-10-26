# HERO VR v0.0.6 - Made by Agent Mulder

VR for Marvel's Spider-Man 2 (Steam, Spider-Man2.exe 2.810.0.0), with the HERO VR launcher (build 6).

**New install:** download `HERO-VR-Launcher-0.0.6.zip` (below), extract it to e.g. `C:\HERO-VR`, double-click
**HERO VR Launcher.cmd**, then **PLAY VR**. The launcher downloads the ready-made mod files
(`HERO-VR-0.0.6-mod-win64.zip`) and checks their SHA-256; nothing is compiled on your PC.
**Already on 0.0.5:** the launcher offers 0.0.6 by itself (or **Updates > Update now**).

## New in 0.0.6
- **Hero powers** (one hero at a time, all OFF by default, no restart needed):
  - IRON MAN: hand repulsor blasts and the CHEST BEAM do real damage. The CHEST BEAM does 4x a blast's damage, has a
    30-second cooldown shown on the wrist ring, and aims at the enemy you look at. (From 0.0.62, hand repulsor blasts
    have their own 2-second cooldown per hand.)
  - DAREDEVIL: a baton in each hand, a trigger throw that chains to up to 3 more enemies within 6 m and comes back,
    harder punches (DAREDEVIL PUNCH 1x-5x, 3x by default) and a sonar look.
  - HULK: a 4x charged jump and one-punch knockouts on normal enemies (bigger ones take 2 to 3).
- **New Iron Man repulsor:** palm charge, a white-hot bolt with a blue falloff, a short impact burst and a new sound
  (about the same cost as the webs).
- **8-page hologram:** STATUS, POWERS, VR SETTINGS, COMBAT, GRAPHICS, COLOR, CONTROLS, MUSIC, each in named sections.
  Settings that need a restart carry a gold RESTART tag.
- **Opt-in reports:** crash reports and Report a bug open a filled-in GitHub issue you read before you submit; play
  stats are off unless you turn them on. Nothing is ever sent by itself.
- **Frame rate:** most players measured 34-37 new frames a second at 90 Hz, limited by the graphics card. After a
  session the mod now says whether the GPU is the limit, suggests 72 or 80 Hz in your VR app when the game already
  makes about half your headset's rate (each picture then stays exactly 2 headset frames: no judder), and suggests
  RenderScale 90 or 80 (about +14% / +30% frames; these are estimates from one RTX 4080 measurement). AUTO
  PERFORMANCE (off by default) now aims at half rate when full rate is out of reach. The graphics defaults are
  unchanged (RenderScale 100, FOVEATED off, AUTO PERFORMANCE off). Details: PERFORMANCE.md in the zip's docs.
- **VR-loss fix:** going back to the desktop in Virtual Desktop (or a headset that drops) no longer leaves the game
  stuck on its flat screen: the mod waits for the headset and brings VR back by itself. PLAY VR waits for a headset
  that is not connected yet and offers to close a Spider-Man 2 left running.

## Going back to 0.0.5
Open **Revert** and press **USE PREVIOUS VERSION (0.0.5)**. The launcher downloads the v0.0.5 release from GitHub,
checks its SHA-256, installs it with its ready-made mod files and stays on 0.0.5 (the 0.0.6 offer is skipped). Press
**Updates > Update now** whenever you want 0.0.6 again. You can also skip the 0.0.6 offer and stay on 0.0.5. The
v0.0.5 release and its files stay available.

## Files
- `HERO-VR-Launcher-0.0.6.zip`: the launcher and the full source.
- `HERO-VR-0.0.6-mod-win64.zip`: the ready-made mod files (built by the "Build prebuilt mod" workflow).
- `version.json`: the launcher's update manifest. `SHA256SUMS.txt` and the `.sha256` files: checksums.

## Credits and license
Based on **Spidy (SpidyVR) by Ilya Mezerowsky** (https://github.com/IlyaMez/SpidyVR), under the GNU GPL v3. HERO VR
is GPLv3 too: see LICENSE, CREDITS.md and THIRD_PARTY_NOTICES.md. Full changes: CHANGELOG.md.

Not affiliated with Insomniac Games, Sony, Marvel or Valve.
