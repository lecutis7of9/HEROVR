# ULTIMATE ALLIANCE EXPANSION (M1: Stark Tower)

You can walk Marvel: Ultimate Alliance (2006) places in VR inside HERO VR, on foot, from the wrist hologram's
**ULTIMATE ALLIANCE EXPANSION** page. The first place is **Stark Tower**, the Act 1 hub (`stark1`). It works the
same way as the Batcave trip: the mod draws the room itself, and the game is hidden while you are there.

## Bring your own game

HERO VR ships **no** Ultimate Alliance data. You build the room from **your own** MUA1 PS2 disc image (the one you
play in PCSX2) with `tools/ua/convert_ua.py`. The converted files go only into your own HERO VR folder
(`Models/UA/StarkTower`). They are never committed: `.gitignore` covers `/Models/UA/`, `*.iso` and `assetsfb.wad`.

### What you need (once)

- Python 3.10+ with `pip install numpy pillow pycdlib raven-formats`
- Blender 4.4 (the portable zip is fine)
- Kaiko's igb-blender add-on: <https://github.com/KaikoClanworth1/igb-blender> (download it; you pass the
  converter the folder that holds its `__init__.py`; you do not install it in Blender yourself)

### Convert

    python tools/ua/convert_ua.py --iso "D:/PS2/Marvel - Ultimate Alliance (USA).iso" ^
        --blender "C:/Blender 4.4/blender.exe" --igb-blender "D:/tools/igb-blender" ^
        --out "<your HERO VR folder>/Models/UA/StarkTower" --previews

The converter does the following:

1. Copies `Z/ASSETSFB.WAD` out of the disc image (read-only; the image is never changed). If you already have
   that file, pass it with `--wad`.
2. Unpacks the `stark1` package (`__combined.igb` and the map's `stark1.engb`).
3. In Blender (`tools/ua/build_stark_tower.py`):
   - Imports the hub with the add-on's `mua1_ps2` profile.
   - Scales it to 0.022 m per unit, so the 84-unit character box is about 1.85 m tall.
   - Puts the player start at the origin.
   - Raises the city backdrop and pushes it out, so Manhattan and the sky fill the windows at eye height.
   - Leaves the window glass out of the drawing. HERO VR has no see-through materials, so the glass would draw
     black. The glass is still kept as walls in the walk grid.
   - Adds a ceiling made from the hub's own textures: dark panels, gold beams and warm light panels.
4. Builds the walk grid with `tools/hideout074/make_hideout.py` (`mesh` mode on `walk.glb`, which has no backdrop
   and no ceiling).
5. Writes these files to `--out`: `scene.glb`, `room.txt`, `room.json`, `npcs.txt` and `report.txt`. With
   `--previews` it also writes `walk_overlay.png` (the walk grid over the floor plan), `eye_window.png`,
   `eye_spawn.png` and `look_up.png`.

## Play

1. Keep `ua_locations = on` in `live.txt`. It is on by default; switch it `off` to stop loading the place's models.
   Turning it on needs a restart.
2. Start the game. In the console you should see: `UA EXPANSION: STARK TOWER ready (CANDIDATE), 38 characters placed`.
3. Open the wrist hologram and page with the arrows to **ULTIMATE ALLIANCE EXPANSION** (the last page; PREV
   from STATUS gets there). Press **STARK TOWER - ENTER** while you stand on the ground. It refuses mid-air, in a
   vehicle, in a hideout, or on the flat screen.
4. Inside, you start where MUA's player start is:
   - Walk with the left stick and snap-turn 30° with the right stick. You can also walk for real.
   - Spider-Man stays where you left him: the game gets a controller at rest, and the hero tools are off.
   - City sound is ducked while CAVE QUIET is on.
5. **RETURN TO CITY** puts you back exactly where you were. Any of these works:
   - The gold ring by Wyatt Wingfoot's Quinjet (on the lower deck), then A or a trigger
   - The RETURN TO CITY button over your left wrist (poke it with your right hand)
   - Hold B for 0.8 s
   - The page's button again (it reads **STARK TOWER - RETURN TO CITY**)

The page's button tells you why a press does nothing: **NOT CONVERTED** (run the converter, then restart) or
**OFF** (`ua_locations` is off).

## Characters (next update)

`npcs.txt` lists every character spawner in the map, in room space, one per line:
`npc <id> x y z yaw`, for example `npc nickfurynpc01 -2.183 0.000 5.165 90.0`. That makes 38 in the hub, including
Nick Fury, Black Widow, Hank Pym, Wyatt Wingfoot and the S.H.I.E.L.D. agents.

HERO VR already reads the file (`ua_expansion::parseNpcs` into `uaNpcs[]`) and reports the count. The next
update will place the character models at those spots and add talking to them.

## Known limits (CANDIDATE, untested in the headset)

- **Scale** is 0.022 m per unit, judged from character boxes. It may need a small adjustment in the headset
  (`--scale`).
- **Real floor:** the room's floor is set 1.65 m under your eye when you enter, because the game's floor is not read.
- **The game keeps running.** Spider-Man stands still, but enemies nearby can still reach him. Enter when the
  area is quiet.
- **Cost:** about 138,000 triangles drawn unlit, plus 54 texture layers (the model texture array now holds up to
  256). Both are loaded only when `ua_locations` is on and the place has been converted.
