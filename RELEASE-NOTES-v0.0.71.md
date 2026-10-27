# HERO VR 0.0.71 - Made by Agent Mulder

> **EXPERIMENTAL BUILD: 0.0.71 may crash or have issues. 0.0.63 is the most recent STABLE build**, and you can always
> go back to it: in the launcher open **Revert** and press **USE 0.0.63** (your settings are kept). Before updating,
> the launcher asks you to confirm that you understand this.

**0.0.71: fixes VR not starting after Stop VR ("handle is invalid" at finding the headset); includes everything in 0.0.7.**

VR for Marvel's Spider-Man 2 (Steam, Spider-Man2.exe 2.810.0.0), with the HERO VR launcher (0.0.71.1, build 12).
Version 0.0.71 = build Update 39.4. Apart from this fix, 0.0.71 is the same as 0.0.7 (all the new heroes and
everything else in 0.0.7 are included).

**New install:** download `HERO-VR-Launcher-0.0.71.zip` (below), extract it to e.g. `C:\HERO-VR`, double-click
**HERO VR Launcher.cmd**, then **PLAY VR**. The launcher downloads the ready-made mod files
(`HERO-VR-0.0.71-mod-win64.zip`) and checks their SHA-256; nothing is compiled on your PC.
**Already on 0.0.7, 0.0.63, 0.0.62, 0.0.6 or 0.0.5:** the launcher offers 0.0.71 by itself (or **Updates > Update
now**). It installs once and is not offered again.
Before it updates, the launcher shows the EXPERIMENTAL notice above: launcher build 12 (0.0.71) asks you to
press **Yes** to a separate warning; launchers from 0.0.7, 0.0.63 and older show the same warning in their own
update window ("What's new" in the start-up prompt, and in the version name), and you press **Yes** / **OK** there.

## Fixed
- **VR not starting after Stop VR.** In 0.0.7, if you pressed **Stop VR** and then **PLAY VR** again, the launcher
  stopped at "finding the headset" with `[WinError 6] The handle is invalid`, so VR did not start. Stopping VR left
  the launcher with a broken connection that the next start then tried to use. 0.0.71 fixes it: Stop VR no longer
  breaks anything, and the launcher repairs it if it ever happens. A new test runs Stop VR and then PLAY VR again on
  Windows before every release.

## Going back
- Open **Revert** and press **USE 0.0.63**, **USE 0.0.62**, **USE 0.0.6** or **USE 0.0.5**. The launcher downloads
  that release from GitHub, checks its SHA-256 and installs it with its ready-made mod files, then stays there until
  you press **Updates > Update now**. Your settings are kept.
- The older releases and their downloads (including their launchers) stay available.

## Files
- `HERO-VR-Launcher-0.0.71.zip`: the launcher and the full source.
- `HERO-VR-0.0.71-mod-win64.zip`: the ready-made mod files (built by the "Build prebuilt mod" workflow; 0.0.7's mod plus the Stop VR fix).
- `version.json`: the launcher's update manifest. `SHA256SUMS.txt` and the `.sha256` files: checksums.

## Credits and license
Based on **Spidy (SpidyVR) by Ilya Mezerowsky** (https://github.com/IlyaMez/SpidyVR), under the GNU GPL v3. HERO VR
is GPLv3 too: see LICENSE, CREDITS.md and THIRD_PARTY_NOTICES.md. Full changes: CHANGELOG.md.

Not affiliated with Insomniac Games, Sony, Marvel or Valve.
