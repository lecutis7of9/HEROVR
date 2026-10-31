# HERO VR 0.0.75 - Made by Agent Mulder

## Install in 3 steps
1. **Download `HERO-VR-Launcher-0.0.75.zip`** from *Assets* below (not "Source code": that is only the source).
2. **Right-click it > Extract All...** and open the **HERO-VR-Launcher-0.0.75** folder (if Windows puts it inside
   another folder of the same name, open that one too).
3. **Double-click `HERO VR Launcher.cmd`**, then press **PLAY VR**. That is the only file to open.

> **EXPERIMENTAL BUILD: 0.0.75 may crash or have issues. 0.0.63 is the most recent STABLE build.** You can always go
> back: in the launcher open **Revert** and press **USE 0.0.74** or **USE 0.0.63** (your settings are kept). Before
> updating, the launcher asks you to confirm that you understand this.

**0.0.75: a motorcycle for Captain America and Wolverine, a Spider-Man 1 option in the launcher, fixes for
supersonic flight, Daredevil and Captain America grapples, and every outfit unlocked.**

VR for Marvel's Spider-Man 2 (Steam, Spider-Man2.exe 2.810.0.0), with the HERO VR launcher (0.0.75.1, build 16).
Version 0.0.75 = build Update 39.8.

**New install:** the 3 steps above. On the first PLAY VR the launcher downloads the ready-made mod files
(`HERO-VR-0.0.75-mod-win64.zip`) and checks their SHA-256; nothing is compiled on your PC.
**Already on 0.0.74 or older:** the launcher offers 0.0.75 by itself (or **Updates > Update now**), after the
EXPERIMENTAL notice above.

## What's new
- **Supersonic fixed:** flight and the shockwave follow your fist, and you stay level after the coast.
- **Only Spider-Man wall-crawls.** Other heroes no longer run up or stick to walls (Wolverine with claws out and
  CLAW CLIMB on can still climb).
- **Daredevil:** press **A** during his grapple to jump out of it.
- **Captain America** gets Daredevil's quick grapple.
- **Motorcycle for Captain America and Wolverine:** tap **X** and it arrives where you look. Grip the left
  handlebar to get on, turn the bars to steer, ride with the triggers. It is driver only (no passenger seat).
- **B** while seated recenters you in the seat.
- **Models hide behind walls** properly (better depth detection).
- **More enemy types** take damage from the heroes.
- **Thor's hammer throw** no longer lunges you forward.
- **All outfits are unlocked** from the start.
- **Launcher: Spider-Man 1 option.** Pick SPIDER-MAN 2 (HERO VR) or SPIDER-MAN 1. Spider-Man 1 runs **iilyn's
  SpidyVR** (by Ilya Mezerowsky, https://github.com/IlyaMez/SpidyVR), downloaded from his GitHub, unchanged and
  credited. The heroes add-on for Spider-Man 1 is still in progress.
- **Cleaner reports:** only a crash dump from this session is attached.
- **SM1STYLE hint:** if the game's Anti-Aliasing is not TAA, the launcher tells you to set it to TAA.

## Known issues
- Aliasing and window edges are still being improved.
- Damage to the green symbiote enemies is not confirmed yet.
- The '89 Batmobile can look grey.
- The motorcycle and the Spider-Man 1 option are untested in a headset.
- No passenger seats.

## Going back
- Open **Revert** and press **USE 0.0.74** or **USE 0.0.63** (or 0.0.73, 0.0.62, 0.0.6, 0.0.5). The launcher
  downloads that release, checks its SHA-256 and installs it. Your settings are kept.

## Files
- `HERO-VR-Launcher-0.0.75.zip`: the launcher, the full source, `Models/` (every model with its licence, now with
  the motorcycle) and `Sounds/`.
- `HERO-VR-0.0.75-mod-win64.zip`: the ready-made mod files (built by the "Build prebuilt mod" workflow).
- `version.json`: the launcher's update manifest. `SHA256SUMS.txt` and the `.sha256` files: checksums.
- `collect-reports.ps1`: gathers logs for a bug report.

## Credits
Made by Agent Mulder. Based on **Spidy (SpidyVR) by Ilya Mezerowsky (iilyn)** (https://github.com/IlyaMez/SpidyVR),
under the GNU GPL v3; the Spider-Man 1 option runs his mod unchanged. HERO VR is GPLv3 too: see LICENSE, CREDITS.md
and THIRD_PARTY_NOTICES.md. Full changes: CHANGELOG.md.

New model: "Low-Poly Sci-Fi Motorcycle - Cyberpunk Theme"
(https://sketchfab.com/3d-models/low-poly-sci-fi-motorcycle-cyberpunk-theme-50c38758e5134a0b8ffa49780e7fe228) by
**Mert Tetik** (https://sketchfab.com/merttetik), CC BY 4.0 (http://creativecommons.org/licenses/by/4.0/), converted
for VR. Every other model is credited to its artist in THIRD-PARTY-MODELS.md, CREDITS.md and the launcher's Credits
page; each model folder ships its own `license.txt`. Some models are **non-commercial**: HERO VR is free, never sold,
paywalled or ad-supported.

Not affiliated with Insomniac Games, Sony, Marvel, DC, Warner Bros. or Valve.
