# HERO VR v0.0.63 - Made by Agent Mulder

VR for Marvel's Spider-Man 2 (Steam, Spider-Man2.exe 2.810.0.0), with the HERO VR launcher (0.0.63.1, build 10).
Version 0.0.63 = build Update 39.3. **0.0.63 fixes 0.0.62 failing to start VR** (the 'session_off' crash); apart
from that fix the mod is the same as 0.0.62, and the launcher has a new optional design.

**New install:** download `HERO-VR-Launcher-0.0.63.zip` (below), extract it to e.g. `C:\HERO-VR`, double-click
**HERO VR Launcher.cmd**, then **PLAY VR**. The launcher downloads the ready-made mod files
(`HERO-VR-0.0.63-mod-win64.zip`) and checks their SHA-256; nothing is compiled on your PC.
**Already on 0.0.62, 0.0.6 or 0.0.5:** the launcher offers 0.0.63 by itself (or **Updates > Update now**). It
installs once and is not offered again.

## Fixed
- **0.0.62 failing to start VR (the 'session_off' crash).** In 0.0.62, PLAY VR stopped at "starting the game"
  with `NameError: name 'session_off' is not defined`, so VR never started. 0.0.63 fixes it. A new test now runs the
  real launch path before every release, so this kind of mistake cannot ship again.

## What's new
- **A new launcher design (optional).** A full-screen-feeling HERO VR window with the glowing orb, a soundscape that
  plays while the launcher is open, and big PLAY VR / FLATSCREEN / UPDATES / OPTIONS / REVERT buttons. The sound
  always stops when the game starts, and the animation pauses while you play.
- **You choose.** The launcher still opens in the classic design. The first time, it asks once whether you want to try
  the new one: **Restart with new design** or **Not now**. You can change it any time in **Options > Launcher > New
  launcher design**; **Launcher sound** turns the soundscape off.
- **Back to classic any time.** In the new design, **Options > Switch to classic launcher**. Hold **Shift** while the
  launcher opens (or start it with `--classic`) to open the classic design just that once.
- **Safety net.** If the new design ever can't start on your PC (a damaged file, a graphics problem, or it stops
  responding), the launcher opens the classic design instead and tells you why, once.
- **Revert** now also offers **USE 0.0.62**.
- Everything else - updates, repair, reports, your settings - works as in 0.0.62, and PLAY VR starts again.

## Going back
- **To the classic launcher design:** Options > Launcher > New launcher design off (or Switch to classic launcher).
- **To an older version:** open **Revert** and press **USE 0.0.62**, **USE 0.0.6** or **USE 0.0.5**. The launcher
  downloads that release from GitHub, checks its SHA-256 and installs it with its ready-made mod files, then stays
  there until you press **Updates > Update now**. Your settings are kept.
- The v0.0.62, v0.0.6 and v0.0.5 releases and their downloads (including their launchers) stay available.

## Files
- `HERO-VR-Launcher-0.0.63.zip`: the launcher and the full source.
- `HERO-VR-0.0.63-mod-win64.zip`: the ready-made mod files (built by the "Build prebuilt mod" workflow; 0.0.62's mod plus the launch fix).
- `version.json`: the launcher's update manifest. `SHA256SUMS.txt` and the `.sha256` files: checksums.

## Credits and license
Based on **Spidy (SpidyVR) by Ilya Mezerowsky** (https://github.com/IlyaMez/SpidyVR), under the GNU GPL v3. HERO VR
is GPLv3 too: see LICENSE, CREDITS.md and THIRD_PARTY_NOTICES.md. Launcher font: Barlow (SIL Open Font License,
tools/launcher/assets/hero/OFL-Barlow.txt). Repulsor and chest beam sounds: made from a sound sent in by a HERO VR player.
Full changes: CHANGELOG.md.

Not affiliated with Insomniac Games, Sony, Marvel or Valve.
