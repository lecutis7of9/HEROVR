# HERO VR 0.0.72 - Made by Agent Mulder

## Install in 3 steps
1. **Download `HERO-VR-Launcher-0.0.72.zip`** from *Assets* below (not "Source code": that is only the source).
2. **Right-click it > Extract All...** and open the **HERO-VR-Launcher-0.0.72** folder (if Windows puts it inside
   another folder of the same name, open that one too).
3. **Double-click `HERO VR Launcher.cmd`**, then press **PLAY VR**. That is the only file to open.

> **EXPERIMENTAL BUILD: 0.0.72 may crash or have issues. 0.0.63 is the most recent STABLE build**, and you can always
> go back to it: in the launcher open **Revert** and press **USE 0.0.63** (your settings are kept). Before updating,
> the launcher asks you to confirm that you understand this.

**0.0.72: THOR and GREEN LANTERN, real 3D models for every hero weapon, 23 worn suits, reputation and perks; includes everything in 0.0.71.**

VR for Marvel's Spider-Man 2 (Steam, Spider-Man2.exe 2.810.0.0), with the HERO VR launcher (0.0.72.1, build 13).
Version 0.0.72 = build Update 39.5.

**New install:** the 3 steps above. On the first PLAY VR the launcher downloads the ready-made mod files
(`HERO-VR-0.0.72-mod-win64.zip`) and checks their SHA-256; nothing is compiled on your PC. The models and suits
(`Models/`) come in the launcher zip.
**Already on 0.0.71, 0.0.7, 0.0.63, 0.0.62, 0.0.6 or 0.0.5:** the launcher offers 0.0.72 by itself (or **Updates >
Update now**). It installs once and is not offered again, and it brings the `Models/` folder with it.
Before it updates, the launcher shows the EXPERIMENTAL notice above: launchers from 0.0.71 and 0.0.72 (builds 12 and 13)
ask you to press **Yes** to a separate warning; older launchers show the same warning in their own update window
("What's new" in the start-up prompt, and in the version name), and you press **Yes** / **OK** there.

## What's new
- **Two new heroes** on the wrist hologram's HERO PROFILES: **THOR** (Mjolnir sheathed on your back: reach back and
  grip to draw it, throw, recall and catch, lightning) and **GREEN LANTERN** (ring flight and constructs). One hero at a
  time, and every hero stays OFF until you pick it.
- **THOR HAMMER** (COMBAT): four real Mjolnir models, #1 BOOOOP (the default), #5 DEVILSEYE, #6 LOVE THUNDER and #3 FCCOOPER.
- **Real 3D models**: the batons, batarangs, guns, grapple gun and shield now use the artists' own geometry, UVs and
  textures, no longer flattened or one colour per triangle. A missing model shows MODEL MISSING, never a stand-in.
- **23 worn suits** (the ROSTER page on the wrist): pick a suit for each hero. A worn suit takes the game body's place
  (SUIT BODY: ARMS, FULL BODY or OFF) and shows in MIRROR VIEW. The default is still THE GAME'S OWN suit.
- **Reputation, skill trees and perks** for every hero.
- **GRAPPLE** for BATMAN, DAREDEVIL and the PUNISHER: takeoff from the ground, a sustained pull and a line drawn.
  BATMAN's trigger fires the grapple, a grip on a belt pouch draws a batarang, and his cape fans out while gliding.
- **SUPERMAN**: two thin heat-vision beams from the eyes, with sparks, a scorch mark and a looping sound.
  **PUNISHER**: drop-leg thigh holsters, the barrel along the aim, a muzzle flash and the player's own shotgun, pistol
  and reload sounds. **CAPTAIN AMERICA**: the thrown shield always flies home onto the forearm.
  **WOLVERINE**: grip-fist claws with SNIKT. **THOR**'s lightning uses the player's two lightning sounds.
- **WINDOW LIGHTS**: LOD FADE is off by default, and the game view's lens is never narrower than both eyes.
- The phone is Spider-Man only, and putting it back in the back pocket works (no stuck phone).
- **PUNISHER VAN (EXPERIMENTAL, OFF by default)**: tap X as the Punisher to summon it. It is an engine preview only:
  the van does not drive you through the city yet.
- The launcher (0.0.72.1, build 13) credits every model and suit artist. Before it updates, it still asks you to
  confirm that the new version is EXPERIMENTAL and that 0.0.63 is the most recent STABLE build. **Revert** offers
  **USE 0.0.63**, 0.0.62, 0.0.6 and 0.0.5.

## Going back
- Open **Revert** and press **USE 0.0.63**, **USE 0.0.62**, **USE 0.0.6** or **USE 0.0.5**. The launcher downloads
  that release from GitHub, checks its SHA-256 and installs it with its ready-made mod files, then stays there until
  you press **Updates > Update now**. Your settings are kept.
- The older releases and their downloads (including their launchers) stay available.

## Files
- `HERO-VR-Launcher-0.0.72.zip`: the launcher, the full source, `Models/` (every weapon, hammer, van, ROSTER and suit
  model, each with its licence) and `Sounds/`.
- `HERO-VR-0.0.72-mod-win64.zip`: the ready-made mod files (built by the "Build prebuilt mod" workflow).
- `version.json`: the launcher's update manifest. `SHA256SUMS.txt` and the `.sha256` files: checksums.

## Credits and licences
Made by Agent Mulder. Based on **Spidy (SpidyVR) by Ilya Mezerowsky** (https://github.com/IlyaMez/SpidyVR), under the
GNU GPL v3. HERO VR is GPLv3 too: see LICENSE, CREDITS.md and THIRD_PARTY_NOTICES.md. Full changes: CHANGELOG.md.

The 3D models belong to their artists and are credited by name, link and licence in THIRD-PARTY-MODELS.md, CREDITS.md
and the launcher's Credits page; each model folder ships its own `license.txt`. Most are CC BY 4.0. Some are
**non-commercial**: floh's two Punisher suits (CC BY-NC 4.0), Nathang30's Iron Man Mark 1 (CC BY-NC-SA 4.0,
share-alike) and baton268's JOKER batarang (CC BY-NC-ND 4.0, unmodified). Tigerar1's Batman Year One and Marco.41's
Captain America WW2 suits are under the Sketchfab Standard licence (https://sketchfab.com/licenses). HERO VR is free:
it is never sold, paywalled or ad-supported, and these models must not be used commercially.

Not affiliated with Insomniac Games, Sony, Marvel, DC or Valve.
