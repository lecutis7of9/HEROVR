# HERO VR 0.0.73 - Made by Agent Mulder

## Install in 3 steps
1. **Download `HERO-VR-Launcher-0.0.73.zip`** from *Assets* below (not "Source code": that is only the source).
2. **Right-click it > Extract All...** and open the **HERO-VR-Launcher-0.0.73** folder (if Windows puts it inside
   another folder of the same name, open that one too).
3. **Double-click `HERO VR Launcher.cmd`**, then press **PLAY VR**. That is the only file to open.

> **EXPERIMENTAL BUILD: 0.0.73 may crash or have issues. 0.0.63 is the most recent STABLE build**, and you can always
> go back to it: in the launcher open **Revert** and press **USE 0.0.63** (your settings are kept). Before updating,
> the launcher asks you to confirm that you understand this.

**0.0.73: the hero ROSTER on the hologram, a PUNISHER VAN that drives, weapons that hit the world, an 80 m grapple, Wolverine's claws, steadier street lights, and GAME TAA OFF by default; includes everything in 0.0.72.**

VR for Marvel's Spider-Man 2 (Steam, Spider-Man2.exe 2.810.0.0), with the HERO VR launcher (0.0.73.1, build 14).
Version 0.0.73 = build Update 39.6.

**New install:** the 3 steps above. On the first PLAY VR the launcher downloads the ready-made mod files
(`HERO-VR-0.0.73-mod-win64.zip`) and checks their SHA-256; nothing is compiled on your PC. The models and suits
(`Models/`) come in the launcher zip.
**Already on 0.0.72, 0.0.71, 0.0.7, 0.0.63, 0.0.62, 0.0.6 or 0.0.5:** the launcher offers 0.0.73 by itself (or **Updates >
Update now**). It installs once and is not offered again, and it brings the `Models/` folder with it.
Before it updates, the launcher shows the EXPERIMENTAL notice above: launchers from 0.0.71, 0.0.72 and 0.0.73 (builds 12, 13 and 14)
ask you to press **Yes** to a separate warning; older launchers show the same warning in their own update window
("What's new" in the start-up prompt, and in the version name), and you press **Yes** / **OK** there.

## What's new
- **GAME TAA starts OFF** (wrist GRAPHICS > QUALITY, under AA). The 0.0.73 test builds ran the game's own TAA in each
  eye (SPIDYVR) by default, and for some players that made the picture shake in motion. 0.0.73 starts with it OFF and
  leaves the game's own anti-aliasing setting alone. One press switches it back to SPIDYVR, with no restart. While
  GAME TAA is SPIDYVR (and ENGINE TAA is on in the launcher), VR sets the game's AA to TAA with no upscaler and puts
  yours back afterwards, unless the launcher's KEEP MY GAME AA is on. If you played a 0.0.73 test build, GAME TAA moves
  to OFF once. Pick SPIDYVR again and it stays.
- **The ROSTER is on the hologram**: the wrist's ROSTER opens hero tiles (MARVEL and DC tabs) with their portraits. Each
  hero has a page with PLAY AS, CONTROLS (every button explained), OUTFITS (his suits), STATS (rep, rank, level and the
  rep log) and SETTINGS (his old POWERS options). The tiles no longer flicker.
- **The PUNISHER VAN drives** (ON by default, can be switched off). Getting in no longer swings the van round. Grip the
  wheel and that hand's trigger drives (pull its stick back to reverse), while your free hand shoots. It has engine
  and door sounds and a dash screen (MAP, VEHICLE STATUS, CASES, LOCKER) that X or a poke pages through. The van is
  also much lighter on the CPU.
- **WEAPON COLLISION** (ON): thrown and held weapons (batons, shield, hammer, batarangs) hit walls, cars and the
  ground, bounce off and come home. Held weapons stop short of a surface. Weapons show in the mirror.
- **Grapple for every grappler**: the grip draws it and the trigger fires it, and it now reaches 80 m (it was 45 m).
  DAREDEVIL's grapple fires on a normal trigger press, and his silver baton tip flies on a rope to where you aim. His
  batons sit properly in your fist and flip and throw freely. The PUNISHER can grab with his hook. CAPTAIN AMERICA's
  shield stays on his arm, and he can sprint.
- **WOLVERINE**: retractable claws for every Wolverine suit, rooted in the fist and level with the hand (CLAW LEVEL).
- **IRON MAN**: the repulsor fires straight out of the open palm again (PALM BLAST), the repulsor beam is fixed, and
  HERO HANDS gives him his own hands, palm out when raised.
- Web lines and wall climbing are Spider-Man only.
- **Body**: the hologram's body calibration uses a true T-pose and fits the holsters, the holsters turn with your body,
  and flying leans with your speed (worn suits too).
- **Picture**: fewer flickering street lights (LIGHT HOLD, SHADOW MATCH) and far windows (CULL MARGIN), buildings keep
  flat play's detail in VR (WINDOW POP FIX, up to 2x), and EYE BLOOM is OFF and LENS FLARES are OFF IN VR by default
  (no strobing halos or headlight ghosts). IMAGE SPIDYVR is sharp with no smear; CLASSIC is the older look.
- **STREAMER MODE** (ON): the monitor shows the headset's view, with everything the mod draws, steadied (STREAMER
  SMOOTH) and from the eye you pick (STREAMER EYE).
- **SMOOTH TURN is on by default** (120°/s). Set it to 0 for snap turning. A 0 saved by an older launcher as its own
  default moves to 120 once.
- The launcher (0.0.73.1, build 14) opens the classic design by default again (the new design stays a switch in
  Options > Launcher), adds KEEP MY GAME AA, and credits Wolverine's claws. Before it updates, it still asks you to
  confirm that the new version is EXPERIMENTAL and that 0.0.63 is the most recent STABLE build. **Revert** offers
  **USE 0.0.63**, 0.0.62, 0.0.6 and 0.0.5.
- Not in this release: the multiplayer test (MP SOLO TEST) from the test builds.

## Going back
- Open **Revert** and press **USE 0.0.63**, **USE 0.0.62**, **USE 0.0.6** or **USE 0.0.5**. The launcher downloads
  that release from GitHub, checks its SHA-256 and installs it with its ready-made mod files, then stays there until
  you press **Updates > Update now**. Your settings are kept.
- The older releases and their downloads (including their launchers) stay available.

## Files
- `HERO-VR-Launcher-0.0.73.zip`: the launcher, the full source, `Models/` (every weapon, hammer, van, ROSTER, claw and suit
  model, each with its licence) and `Sounds/`.
- `HERO-VR-0.0.73-mod-win64.zip`: the ready-made mod files (built by the "Build prebuilt mod" workflow).
- `version.json`: the launcher's update manifest. `SHA256SUMS.txt` and the `.sha256` files: checksums.

## Credits and licences
Made by Agent Mulder. Based on **Spidy (SpidyVR) by Ilya Mezerowsky** (https://github.com/IlyaMez/SpidyVR), under the
GNU GPL v3. HERO VR is GPLv3 too: see LICENSE, CREDITS.md and THIRD_PARTY_NOTICES.md. Full changes: CHANGELOG.md.

The 3D models belong to their artists and are credited by name, link and licence in THIRD-PARTY-MODELS.md, CREDITS.md
and the launcher's Credits page; each model folder ships its own `license.txt`. New in 0.0.73: WOLVERINE's claws, cut from Light.k's "Wolverine - Jacket -
Jeans (Rigged and Textured)" (CC BY 4.0). Most are CC BY 4.0. Some are
**non-commercial**: floh's two Punisher suits (CC BY-NC 4.0), Nathang30's Iron Man Mark 1 (CC BY-NC-SA 4.0,
share-alike) and baton268's JOKER batarang (CC BY-NC-ND 4.0, unmodified). Tigerar1's Batman Year One and Marco.41's
Captain America WW2 suits are under the Sketchfab Standard licence (https://sketchfab.com/licenses). HERO VR is free:
it is never sold, paywalled or ad-supported, and these models must not be used commercially.

Not affiliated with Insomniac Games, Sony, Marvel, DC or Valve.
