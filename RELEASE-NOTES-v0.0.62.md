# HERO VR v0.0.62 - Made by Agent Mulder

VR for Marvel's Spider-Man 2 (Steam, Spider-Man2.exe 2.810.0.0), with the HERO VR launcher (build 9).
Version 0.0.62 = build Update 39.2, on top of 0.0.6 (and its hotfix).

**New install:** download `HERO-VR-Launcher-0.0.62.zip` (below), extract it to e.g. `C:\HERO-VR`, double-click
**HERO VR Launcher.cmd**, then **PLAY VR**. The launcher downloads the ready-made mod files
(`HERO-VR-0.0.62-mod-win64.zip`) and checks their SHA-256; nothing is compiled on your PC.
**Already on 0.0.6 or 0.0.5:** the launcher offers 0.0.62 by itself (or **Updates > Update now**).

## Fixed
- **Ghost copies of your hands:** the EYE AA smoothing no longer leaves faint copies behind when your hands move fast.
- **Repulsor bolts, the chest beam and web balls went through walls:** they now stop at the first wall, with their
  impact flash there.
- **Headlights and bridge lights shining through walls at night:** these are the game's own lens flares. New option
  **LENS FLARES** (GRAPHICS, needs a restart of VR): OFF IN VR turns them off while VR runs. GAME (default) leaves them.
- **Combat lag:** **PUNCH HITS** (default GAME + INSTANT) also lands a small hit the moment your fist touches, and
  **ENEMY TRACKING** (default LIVE) follows enemies 30 times a second by their own position. The 0.0.6 behaviour is
  one setting away for both.
- **Daredevil's batons did no damage:** hits in the hand and straight throws now hurt enemies.

## New
- **Your repulsor sound:** Iron Man's hand blast now uses the repulsor sound sent in by a player - thank you! (Its
  original source is not known to us; if it comes from a film or game it will be replaced.)
- **New chest beam sound and plasma look:** the CHEST BEAM's sound is made from the same player's repulsor
  sound (deeper, with a boom), and the hand bolts and the chest beam now shimmer like plasma.
- **PALM BLAST:** the hand repulsor fires out of your palm, in the direction the palm faces, like in the films. Web
  shooters are unchanged.
- **Hand cooldown:** hand repulsor blasts have a 2-second cooldown per hand (each hand's own timer, from its trigger
  pull), shown as a ring on each wrist. The CHEST BEAM is separate: 4x a blast's damage, 30-second cooldown shown on
  the wrist ring, as in 0.0.6.
- **Baton bounce:** a thrown baton hits up to 3 enemies (each within 5 m of the last, never the same one twice) and
  always comes back.
- **REAL ANCHORS** (launcher, ON by default): webs only hold on a real building or object; a web into empty sky misses
  with a short buzz. Turn it off for the old swing in thin air.
- **BEACONS IN VISION** (DAREDEVIL, ON by default): mission beacons and markers stay bright and coloured in the sonar.
- **HIT INDICATOR** (COMBAT, OFF by default) and **HAND FLIPS** (COMBAT, ON by default as in 0.0.6).
- **Launcher build 9:** Revert offers **USE 0.0.6** and **USE 0.0.5**; versions compare as numbers. The first run
  explains a zip that was not extracted, a folder name with brackets, or an antivirus removing the launcher.

The graphics defaults are unchanged (RenderScale 100, FOVEATED off, AUTO PERFORMANCE off, LENS FLARES GAME).

## Checked before release

Every problem players have reported so far now has an automatic check that runs before each release, so it can't quietly come back: the Windows build break (min/max), launcher crashes on older .NET, a program or zip-inside-a-zip in the Nexus download, a release without the ready-made mod, garbled launcher logs, locked old files during an update, the Microsoft Store "python" stub, VR going flat after Virtual Desktop or a refused eye image, the update removing the mod's Python or leaving older mod files, a blank repository setting, folders with brackets or running inside the zip, and self-update and Revert from every launcher released so far (builds 3, 4, 6, 7 and 8).

## Going back
Open **Revert** and press **USE 0.0.6** or **USE 0.0.5**. The launcher downloads that release from GitHub, checks its
SHA-256 and installs it with its ready-made mod files, then stays there until you press **Updates > Update now**.
The v0.0.6 and v0.0.5 releases and their files stay available.

## Files
- `HERO-VR-Launcher-0.0.62.zip`: the launcher and the full source.
- `HERO-VR-0.0.62-mod-win64.zip`: the ready-made mod files (built by the "Build prebuilt mod" workflow).
- `version.json`: the launcher's update manifest. `SHA256SUMS.txt` and the `.sha256` files: checksums.

## Credits and license
Based on **Spidy (SpidyVR) by Ilya Mezerowsky** (https://github.com/IlyaMez/SpidyVR), under the GNU GPL v3. HERO VR
is GPLv3 too: see LICENSE, CREDITS.md and THIRD_PARTY_NOTICES.md. Repulsor and chest beam sounds: made from a sound sent in by a HERO VR player.
Full changes: CHANGELOG.md.

Not affiliated with Insomniac Games, Sony, Marvel or Valve.
