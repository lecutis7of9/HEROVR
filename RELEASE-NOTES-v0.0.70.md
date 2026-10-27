# HERO VR 0.0.7 - Made by Agent Mulder

Five new heroes, guns that shoot where they point, and a smoother launcher.

VR for Marvel's Spider-Man 2 (Steam, Spider-Man2.exe 2.810.0.0), with the HERO VR launcher (0.0.70.1, build 11).

**Why the tag says v0.0.70:** this is 0.0.7. The launcher compares versions number by number, so "0.0.7" would look
older than 0.0.63 (7 is less than 63) and nobody would be offered it. 0.0.70 is the same release with a trailing 0,
so every launcher sees it as newer than 0.0.63.

**New install:** download `HERO-VR-Launcher-0.0.70.zip` (below), extract it to e.g. `C:\HERO-VR`, double-click
**HERO VR Launcher.cmd**, then **PLAY VR**. The launcher downloads the ready-made mod files
(`HERO-VR-0.0.70-mod-win64.zip`) and checks their SHA-256; nothing is compiled on your PC.
**Already on 0.0.63, 0.0.62, 0.0.6 or 0.0.5:** the launcher offers 0.0.7 by itself (or **Updates > Update now**). It
installs once and is not offered again.

## What's new
- **New heroes:** **BATMAN** (batarangs in each hand that bounce between enemies and come back, smoke, a grapple gun and
  a cape glide - see below),
  **SUPERMAN** (heat vision, COLD BREATH, super strength, the SUPERSONIC dash), **PUNISHER** (two pistols and a
  sawed-off, hip and back holsters, reload), **WOLVERINE** (claws - a claws-out hit is 3x a punch - and healing) and **CAPTAIN AMERICA** (his shield:
  wear it, draw it from your back, guard, throw it so it bounces and comes back, bash with it). Find them on the wrist
  hologram's COMBAT page under HERO PROFILES. One hero at a time.
- **Batman's grapple gun and cape glide.** Aim a hand up at a rooftop or ledge and pull the trigger: the grapple
  line fires and zips you up and over the edge (aimed level or at an enemy, the trigger throws a batarang as before).
  In the air, hold your **arms out** to glide on the cape: dip one arm (raise the other) to turn, **tuck your arms**
  to dive, **raise your arms** to pull up and trade speed for height. Drop your arms or land to stop. Batman has no
  webs and no flying.
- **Webs are Spider-Man's only.** With any other hero on, there are no web lines, web sounds or web pulls of any kind.
  Iron Man, Superman and Captain America fly; Batman grapples and glides; Daredevil, Punisher and Wolverine run,
  jump and climb. A punch at a far enemy lunges you to him without a web line. Nothing to restart.
- **Everything new is OFF until you turn it on**, including SHIELD BLOCK, which is an experiment.
- **Guns fire along the barrel now.** Punisher's guns sit in your palm and the shot leaves the muzzle you see, so the
  sights are on target. Daredevil's batons sit in your fist.
- **Optional 3D models** for the batons, batarangs, guns and Cap's shield (BATON LOOK, BATARANG STYLE, GUN MODELS,
  SHIELD LOOK). They ship in the `Models` folder. If a file is missing, you get the built-in look.
- **Mirror View runs once and turns itself off.** Turn on the MIRROR VIEW test and it runs for 30 seconds at the next
  VR start, then switches itself off and saves that, so it never stays on by accident. Your choice now also survives
  the restart the wrist asks for.
- **Smoother cutscene starts.** Each time a cutscene started or resumed, the mod used to ask the game for two extra
  3D views for about a second that could never be shown in Spider-Man 2 yet. It doesn't anymore, so that hitch is
  gone. Cutscenes look the same as before (flat on the game screen), and the GAME SCREEN 3D setting now says so.
- **Captain America suit:** the `CapSuit` folder and docs/CAP-SUIT-PIPELINE.md are a guide for swapping the suit by
  hand. It is optional and nothing installs it for you.
- **Launcher build 11:** the optional HERO VR design runs smoother (60 fps menu, the orb loops seamlessly at the
  concept video's speed). The classic design is still the default.
- Valve Index: the 15-degree controller tilt is a guess and is **unconfirmed**. Tell us if the guns or batons point
  high or low.

## Going back
- **One click to 0.0.63:** open **Revert** and press **USE 0.0.63**. The launcher downloads it from GitHub, checks its
  SHA-256 and installs it with its ready-made mod files, then stays there until you press **Updates > Update now**.
  Your settings are kept. **USE 0.0.62**, **USE 0.0.6** and **USE 0.0.5** are there too.
- Or download 0.0.63 yourself: https://github.com/lecutis7of9/HEROVR/releases/tag/v0.0.63
- **The original launcher that works stays up:** the 0.0.63 launcher (`HERO-VR-Launcher-0.0.63.zip`, build 10) is still on
  the v0.0.63 release, unchanged. If the new launcher gives you any trouble, download that one and keep playing.
- The v0.0.63, v0.0.62, v0.0.6 and v0.0.5 releases and their downloads (including their launchers) stay as they are.
- **To the classic launcher design:** Options > Launcher > New launcher design off (or hold **Shift** while it opens).

## Files
- `HERO-VR-Launcher-0.0.70.zip`: the launcher, the full source, the `Models` and `CapSuit` folders.
- `HERO-VR-0.0.70-mod-win64.zip`: the ready-made mod files (built by the "Build prebuilt mod" workflow).
- `version.json`: the launcher's update manifest. `SHA256SUMS.txt` and the `.sha256` files: checksums.

## Credits
- Based on **SpidyVR by Ilya Mezerowsky** (https://github.com/IlyaMez/SpidyVR), under the GNU GPL v3. HERO VR is
  GPLv3 too: see LICENSE, CREDITS.md and THIRD_PARTY_NOTICES.md.
- 3D models, with thanks (CC BY 4.0 unless noted; links and licences in THIRD-PARTY-MODELS.md):
  - **Digital Goblin**: "Captain America Shield" (CC BY 4.0), Cap's shield.
  - **iqbalmulyawan**: "Captain America" (CC BY 4.0), the optional suit guide.
  - **Yanez Designs (Anthony Yanez)**: "Daredevil's baton", Daredevil's batons.
  - **TORI106**: "9mm Pistol", Punisher's pistols.
  - **LiliumLetifer**: "Sawed-off Shotgun", Punisher's sawed-off.
  - **Eira**: "Batarang", batarang EIRA.
  - **Ludus101**: "Batarang Clooney", batarang CLOONEY.
  - **baton268**: "Batarang Joker" (CC BY-NC-ND 4.0), batarang JOKER, shipped unmodified with its licence
    (Models/Batarang/JOKER/license.txt).
- Launcher font: Barlow (SIL Open Font License). Repulsor and chest beam sounds: made from a sound sent in by a
  HERO VR player.
- Full changes: CHANGELOG.md.

Not affiliated with Insomniac Games, Sony, Marvel or Valve.
