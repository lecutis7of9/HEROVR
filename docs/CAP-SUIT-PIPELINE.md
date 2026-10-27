# CAPTAIN AMERICA suit swap: an optional manual guide (Version 0.0.7)

**Optional and manual.** Nothing here is installed by the launcher or the mod: HERO VR never changes the
game's own files. Follow these steps only if you want Peter to wear the Captain America suit; the VR
mod's CAPTAIN AMERICA mode (the shield) works the same without it.

The suit swap is done the way Spider-Man 2 PC modders do it: the model is brought into Blender with
**Luna Engine IO**, rigged and weighted to Peter's skeleton from `hero_spiderman_advanced.model`, exported
back to a `.model`, packed with **Overstrike's Modding Tool** and installed with **Overstrike**. The game's
archives are only on the PC, so the release ships the preparation (a clean mesh, the Blender scripts, tested on
a stand-in rig) and the steps below run on the PC.

The suit is 'Captain America' by iqbalmulyawan
(https://sketchfab.com/3d-models/captain-america-d4898ea3aac3474fa0661de3ade9b7c6), CC BY 4.0 (credit
in THIRD-PARTY-MODELS.md and `CapSuit/CREDITS.txt`). It has no rig. The HERO VR mod does not load it: this is a
game-side asset swap. The VR mod's CAPTAIN AMERICA mode (the shield) works with or without it.

Marks: **CONFIRMED** = read from a real source, named next to it. **CANDIDATE** = not checked against the
game or the tool on the PC; check it there before you rely on it.

## The tools (versions and links)

| Tool | Version | Link | Mark |
|---|---|---|---|
| Blender | 5.0+ (Luna Engine IO asks for 5.0+, 5.0.1 recommended). The scripts were tested headless on 5.2.3 LTS | https://www.blender.org/download/ | CONFIRMED (the Luna Engine IO page) |
| Luna Engine IO Tools (Blender add-on, `.model` import/export) by Pcniado | v2.0.0 Beta | https://www.nexusmods.com/marvelsspiderman2/mods/1925 | CONFIRMED (the Nexus page) |
| Overstrike (mod manager) by Tkachov | OS/v1.9.0 (2026-10-24) | https://github.com/Tkachov/Overstrike/releases, https://www.nexusmods.com/marvelsspiderman2/mods/1 | CONFIRMED (GitHub releases) |
| Overstrike Modding Tool (in the same repository) | MT/v1.4.5 (2026-01-12) | https://github.com/Tkachov/Overstrike/releases | CONFIRMED (GitHub releases) |
| .NET 7.0 Desktop Runtime (Overstrike and the Modding Tool need it) | 7.0.x | https://dotnet.microsoft.com/download/dotnet/7.0 | CONFIRMED (the Overstrike README) |
| The Modding Tool's name list `hashes_i30.txt` (from MT/v1.4.1's `hashes.zip`, sha256 `b6c72c5027249e11de162f429d89dd99d4fb41ab2bccdfa9297513952d3beeff`) | i30 | the MT/v1.4.1 release assets | CONFIRMED |

The game has to be patch 1.526 or later for Luna Engine IO v2.0.0 Beta (CONFIRMED, the Nexus page).

## The archive paths

These are the asset paths as the Modding Tool lists them. The paths come from `hashes_i30.txt`, the
Modding Tool's own name list (see above); each path is shown with its 64-bit asset id. They are
**CONFIRMED as names in that list**. Whether your installed patch still holds each one is CANDIDATE
until you see it in the Modding Tool (step 4).

| What | Path | Id |
|---|---|---|
| Peter's Advanced Suit model (the rig and the mesh you replace) | `characters/hero/hero_spiderman_advanced/hero_spiderman_advanced.model` | A6D53AE572ACCADC |
| its actor | `characters/hero/hero_spiderman_advanced/hero_spiderman_advanced.actor` | B7993FC710C96418 |
| the actor under actors/ | `actors/hero/hero_spiderman_advanced/hero_spiderman_advanced.actor` | 862C2FA6A8D5E14B |
| the red material | `characters/hero/hero_spiderman_advanced/materials/hero_spiderman_advanced_red.material` | 9371086FDDEA5B66 |
| the blue A material | `characters/hero/hero_spiderman_advanced/materials/hero_spiderman_advanced_blue_a.material` | 8A3A82BA2FEC282D |
| the red colour texture | `characters/hero/hero_spiderman_advanced/textures/hero_spiderman_advanced_red_textile_c.texture` | 80418EEC576AB663 |
| the blue A colour texture | `characters/hero/hero_spiderman_advanced/textures/hero_spiderman_advanced_blue_textile_a_c.texture` | 9ED9E917E0CFA165 |
| the red detail mask | `characters/hero/hero_spiderman_advanced/textures/hero_spiderman_advanced_red_textile_detailmask.texture` | BBBBB284AEC5A3C6 |

The list has 97 `hero_spiderman_advanced/` names in all, among them `_blue_b`, `_armor`, `_head`, `_trim`,
`_eye`, `_eyes_frames`, `_white_carbonfiber` and `_dmg_decal` materials. The three texture rows above are
used by the CANDIDATE retexture route in step 5. Which texture each material actually samples is
**CANDIDATE**: open the `.material` in the Modding Tool to see it.

## What is prepared for you (`CapSuit/` in the release zip)

- `CapSuit/cap_suit.gltf` + `cap_suit.bin` + `textures/` contain the cleaned suit. It was cleaned by
  `tools/cap_suit/prep_suit.py` (rerun: `python tools/cap_suit/prep_suit.py <suit source folder> CapSuit`):
  - Y-up, facing +Z (Blender turns that into Z-up, facing -Y).
  - In metres, standing on the ground. It is 1.80 m tall to start (CANDIDATE); the fit script rescales it to the rig.
  - 17186 triangles. Corners are welded and nothing is degenerate.
  - Two material slots: `cap_suit_body` (6242 vertices) and `cap_suit_gear` (4958 vertices). Both are under
    Luna Engine IO's 65535 vertices per submesh. Sketchfab's emissive copy of the colour is dropped.
- `CapSuit/prep-report.json` and `CapSuit/CREDITS.txt`.
- `tools/cap_suit/blender/` holds:
  - `cap_common.py`
  - `fit_suit_to_rig.py` (the auto-fit)
  - `transfer_weights.py` (the weights and the re-pose)
  - `selftest.py`: a headless test on a stand-in A-pose rig. Its result: `SELFTEST PASS`. It checks that
    the suit is scaled and on its feet, that the arms are posed onto the suit's T-pose, and that every
    vertex has 1 to 4 weights summing to 1. It also checks that the fingertips go to the hand bones and
    carry to the rest pose exactly, that the arm length is fitted, and that the T-pose round trip is
    within 1.3 mm mean.

## The steps on the PC (in order)

1. **Back up.** Copy the game's `toc` file (in the game folder, the file the Modding Tool's Load TOC opens; CANDIDATE location) to `toc.vanilla`.
   If you already use Overstrike, it keeps its own copy, so this is extra safety.
2. **Install the tools.**
   - .NET 7.0 Desktop Runtime.
   - Overstrike OS/v1.9.0 and Modding Tool MT/v1.4.5, unzipped anywhere.
   - Blender 5.0.1.
   - Luna Engine IO v2.0.0 Beta: Blender > Edit > Preferences > Add-ons > Install from Disk, choose the
     zip, then tick it.
3. **Names for the Modding Tool.** Put the name list next to `ModdingTool.exe`, renamed from
   `hashes_i30.txt` to `hashes.txt` (a copy is in the MT/v1.4.1 `hashes.zip`).
4. **Extract Peter's model.**
   1. Start the Modding Tool and Load TOC from the game folder.
   2. Find `characters/hero/hero_spiderman_advanced/hero_spiderman_advanced.model` and extract it
      (right click > Extract). (You said this is already done on your PC: reuse that file.)
   3. Also extract the `.material` files listed above, so you can read their texture paths.
5. **Textures (CANDIDATE route).** Spider-Man's materials take colour textures (`*_c.texture`) plus
   detail masks and arrays. The simplest swap:
   - point `cap_suit_body` at `hero_spiderman_advanced_red.material`;
   - point `cap_suit_gear` at `hero_spiderman_advanced_blue_a.material`;
   - replace the `_c.texture` each of those two materials uses with Cap's two colour textures
     (`CapSuit/textures/*.jpeg`, saved as the texture format the Modding Tool's Replace asks for).
   Check in the Modding Tool which `_c.texture` each material really uses (the table above shows the
   likely red and blue_a ones). If the textile detail shows through, replace that material's detail mask
   with a flat black one. All of this is CANDIDATE until it is seen in game.
6. **Blender: import Peter.** In a new scene, use File > Import > Luna Engine IO `.model` and pick
   `hero_spiderman_advanced.model`. This brings in the rig and Peter's meshes. Luna Engine IO's page
   recommends this model as the rig for any suit (CONFIRMED). Save the scene as `cap_rig.blend`.
7. **Blender: auto-fit.** Run the fit script, from a terminal in Blender's folder:
   ```
   blender cap_rig.blend --python <zip>/tools/cap_suit/blender/fit_suit_to_rig.py -- --suit <zip>/CapSuit/cap_suit.gltf
   ```
   (Or open the script in Blender's Scripting tab, set `sys.argv` and Run.) The script:
   1. imports the suit;
   2. scales it to Peter's height and stands it on his feet;
   3. stretches its arms to Peter's arm length;
   4. poses the rig's arms out onto the suit's T-pose.
   Read the `CAP FIT REPORT` line in the console. It shows `arm_bones`, the bones the script picked by
   shape: on each side the upper arm, the forearm and the hand.
   - If the picks are wrong (the game rig's bone names were not available when the scripts were made, so this is
     CANDIDATE), write `bone_map.json`, for example
     `{"upperarm_l": "...", "forearm_l": "...", "hand_l": "...", "upperarm_r": "...", "forearm_r": "...", "hand_r": "..."}`,
     and rerun with `--bones bone_map.json`.
   - If Peter's torso and the suit's still differ, line them up by eye in Edit Mode: select the suit's
     vertices and scale or move them. Do not touch the rig.
8. **Blender: weights.** Run the weight script in the same session, right after the fit. The script
   needs the rig still in the fit pose, so the one-call form is easiest:
   ```
   blender cap_rig.blend --python <zip>/tools/cap_suit/blender/fit_suit_to_rig.py --python <zip>/tools/cap_suit/blender/transfer_weights.py -- --suit <zip>/CapSuit/cap_suit.gltf --save cap_weighted.blend
   ```
   Add `--bones bone_map.json` if step 7 needed it.
   The script:
   1. copies Peter's weights onto the suit (nearest face, interpolated);
   2. keeps the 4 largest weights per vertex and normalises them. `--limit` changes the count. The game's
      own limit is CANDIDATE: Luna Engine IO's page does not state it;
   3. carries the suit from the T-pose to the rig's rest pose;
   4. clears the pose;
   5. parents the suit to the rig with an Armature modifier.
   Check `CAP WEIGHT REPORT` for `unweighted: 0`. Then pose the arms in Pose Mode to look for tears:
   fix them with Weight Paint around the shoulders, the hips and the belt pouches.
9. **Blender: Luna Engine IO's part.** Follow its own page (CONFIRMED order):
   1. The suit's meshes are already parented to the skeleton (step 8).
   2. Fix the weights if needed.
   3. Set each mesh's full `.material` path in Luna Engine IO's panel (the paths from step 5).
   4. Add the meshes to the model's Look group.
   5. Hide or delete Peter's own meshes from that Look, so the suit replaces his body.
   6. Export as `hero_spiderman_advanced.model`.
   Leave **"include STG header" off**: its users report crashes with it on (CONFIRMED, the Nexus page's comments).
10. **Pack.**
    1. In the Modding Tool, select `characters/hero/hero_spiderman_advanced/hero_spiderman_advanced.model`.
    2. Replace it with the exported file. Also replace the textures from step 5.
    3. Use Mod > Pack replaced as .stage and name the result `CaptainAmericaSuit.stage` (CONFIRMED: the
       Modding Tool's menu as its README and Nexus page describe it).
11. **Install.**
    1. Put `CaptainAmericaSuit.stage` in Overstrike's `Mods Library/` folder.
    2. Start Overstrike, tick the mod and press **Install mods**.
    3. Wait for "Mods installed!" (CONFIRMED, the Overstrike README).
    4. Start the game and wear the Advanced Suit: it is Cap.
12. **Rollback.** Untick the mod in Overstrike and press **Install mods** again (CONFIRMED, the README:
    disabling a mod means unticking it and installing again). If the game will not start at all, put
    `toc.vanilla` back as `toc`, or verify the game files in Steam.

## Known limits

- The swap replaces the Advanced Suit, so it is not a new suit slot. A new slot needs a `.suit` mod with
  its own config, which this guide does not cover.
- The source is a game-ready but simple model: one colour atlas for the body and one for the gear, with
  no normal maps. Spider-Man's shaders will light it flatter than Peter's own suit.
- The face is the source model's: Peter's head animation (talking) will not move it. Its weights come
  from Peter's head bones.
- The HERO VR overlay never draws the suit, and the CAP LOOK overlay is not used.
