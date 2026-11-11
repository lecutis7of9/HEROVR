# ULTIMATE ALLIANCE EXPANSION (Stark Tower and its people)

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
   - Scales it to 0.0254 m per unit (Raven's units are inches), so the heroes stand about 1.76 to 1.83 m tall.
     If you converted the room before the characters came, convert again: the room was 0.022 m per unit.
   - Puts the player start at the origin.
   - Raises the city backdrop and pushes it out, so Manhattan and the sky fill the windows at eye height.
   - Leaves the window glass out of the drawing. HERO VR has no see-through materials, so the glass would draw
     black. The glass is still kept as walls in the walk grid.
   - Adds a ceiling made from the hub's own textures: dark panels, gold beams and warm light panels.
4. Builds the walk grid with `tools/hideout074/make_hideout.py` (`mesh` mode on `walk.glb`, which has no backdrop
   and no ceiling).
5. Builds the characters (`tools/ua/ua_cast.py`, posed in Blender by `tools/ua/build_cast.py`):
   - Finds every character spawner in the map, the skin it wears (`npcstat`) and the idle performance its spawn
     script plays (`EA_ZONE*`, from `zone_stark1.igb`): Nick Fury standing by the conference table, Black Widow
     seated at it, Hank Pym at his desk, the Vision, Wyatt Wingfoot by the Quinjet and the office workers.
   - Poses each skin through a few frames of its idle loop (10 for the people you talk to, 6 for the others), with
     the feet on the floor.
   - Packs all of it into `Cast/scene.glb`: every frame whole, one after another, and all their textures in one
     atlas (2 pages of 1024 x 1024). The PS2 models have no LODs; they are about 1,200 to 1,600 triangles each.
6. Reads their conversations (`conversations/act1/stark/*.engb`): every line, Spider-Man's replies and the menu
   choices, who may start which conversation, which question appears after which answer, and the mission a choice
   leads to.
7. Takes the hub's voice bank (`TOWN1_V.ENS`) from the disc and decodes each recorded line it needs (PS-ADPCM) to
   `Voice/v0000.wav`, `Voice/v0001.wav` and so on. With `--wad` instead of `--iso`, pass the bank with `--sounds`;
   without it the characters talk in subtitles only.
8. Writes these files to `--out`: `scene.glb`, `room.txt`, `room.json`, `npcs.txt`, `Cast/scene.glb`, `cast.txt`,
   `talk.txt`, `Voice/` and `report.txt`. With `--previews` it also writes `walk_overlay.png` (the walk grid over
   the floor plan), `eye_window.png`, `eye_spawn.png`, `look_up.png`, `cast_hub.png` (the hub with its
   characters), `cast_floor.png` and `cast_closeup.png` (Nick Fury with a subtitle mock).
   `--no-cast` leaves the characters out.
9. Builds **your body** for UA places (`tools/ua/build_body.py` in Blender, then `tools/ua/ua_body.py`): MUA's own
   Spider-Man (the classic suit, `packages/generated/characters/spiderman_0901.fb`) on its own skeleton, made into a
   HERO VR suit (the 16-bone skin the roster suits use) at `Models/UA/Body/scene.glb`, beside `UA/StarkTower`.
   `--no-body` leaves it out (you get the fallback gloves instead).

## Play

1. Keep `ua_locations = on` in `live.txt`. It is on by default; switch it `off` to stop loading the place's models.
   Turning it on needs a restart.
2. Start the game. In the console you should see:
   `UA EXPANSION: STARK TOWER ready (CANDIDATE), 38 characters, 5 conversations`.
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

**Your body.** In a UA place the game's own picture is hidden, and with it the game's body, so HERO VR draws you
a body of its own. It follows your headset and both controllers: your arms reach your hands, your fingers curl
with the trigger and grip, and your legs step when you walk. You stand on the room's floor. It uses the suit you
wear if you wear a roster suit (IRON MAN, CAP and so on). Otherwise it uses the converted MUA Spider-Man
(`Models/UA/Body`). If neither is there, you get the fallback gloves. You see the full body except the head and
neck. With SUIT BODY OFF you still keep your arms and hands. Combat and powers do not drive it yet.

The page's button tells you why a press does nothing: **NOT CONVERTED** (run the converter, then restart) or
**OFF** (`ua_locations` is off).

## Characters

The hub's people stand where MUA puts them, in their own models, playing their idle loops: Nick Fury, Black Widow,
Hank Pym, the Vision, Wyatt Wingfoot and the office workers. As in the game, a different pair of office workers is
chatting on each visit.

**Talking:**
- Walk up to someone you can talk to (within 3 m) and look or point at him. A tag over his head reads
  **NAME - A: TALK**.
- Press **A** or a trigger. He turns to face you (seated people stay seated) and says his line from MUA. The line
  is shown on a panel in front of him and plays in his recorded voice.
- **A** or a trigger skips to the end of the line.
- Spider-Man's replies and the game's menus ("Are we officially a team?", "Thanks, Colonel.") show as choices. Use
  the left stick up or down, or point at a choice, then press A or a trigger.
- **B** ends the talk. It never ends the trip; hold B again afterwards to go home.
- The conversations play as they do for Spider-Man in MUA. Each greeting plays once a session, and follow-up
  questions appear after the answers that lead to them.

**Missions (coming soon):** Wyatt's "Take us to the Omega Base." asks you to confirm like the game does. **YES**
ends the talk and shows **MISSIONS: COMING SOON**. The missions themselves come in a later update.

On the gold ring by the Quinjet, where Wyatt stands, A goes home. To talk to Wyatt there, point at him instead of
looking at him.

## Fix 2 (after Stark Tower test 1)

- **Black shapes outside the windows, near the ceiling:** the black box that hides the game's picture was pulled
  inside the far plane one corner at a time, so its faces bent in through the sky and the skyline. A UA place now
  reaches 190 m (the overlay sees 200 m). Any room bigger than that gets a black sphere round your eye instead of
  a box, and the sphere never cuts through the room. The converter also closes the top of MUA's sky box (it had
  none, since the game's camera never looked up), so the glass under the ceiling shows sky instead of black.
- **Flicker on the furniture:** the room's models are now drawn relative to your eye (the position offsets are
  worked out in double precision on the CPU). The GPU no longer does float maths on city coordinates kilometres
  from the origin. The converter also drops exact duplicate faces (the map places some pieces several times in
  the same spot; about 2,400 faces).
- **No body or hands:** see **Your body** above.

## Fix 3 (after Stark Tower test 2): your own Spider-Man

- **Your SM2 hero, not MUA's.** In a UA place the game keeps drawing its own hero (native_body poses him to the
  headset and controllers; the eyes are in his head), and the room drawn over the game's picture used to hide him.
  Now the overlay cuts the black box and the room where the game's own depth puts a surface inside a body-shaped
  volume round you (capsules: torso, arms to the controllers, legs to the game's ground; no head) and above your feet
  (`include/spidy/ua_body_cut.hpp`, `D3D12Renderer::setBodyCut`). Room furniture in front of you still covers him.
- GAME BODY is switched on for the visit (whatever its setting); a worn HERO VR suit still wins, as outside.
- **Fallback:** MUA Spider-Man (Models/UA/Body) only while the game's body is not drawn (its address checks failed,
  a hero without one), or when the cut never comes through (no scene depth for ~3 s: the rest of the session).
  The fallback no longer counts as a worn suit, so it never keeps the game's body off. The gloves only when neither shows.
- **Flicker:** the IGB import marked every room material BLEND, so stray clear texels in solid furniture were cut as
  holes. `tools/ua/ua_alpha.py` (run by the converter) writes materials with under 2 % clear texels OPAQUE (their
  alpha ignored, no discard); the plants, leaves, water, glass and soft shadows keep their cut-outs.
- The console's scene-depth line shows `UA own-hero cut N` (frames drawn) and any fault.

## Known limits (CANDIDATE, untested in the headset)

- **Scale** is 0.0254 m per unit (an inch), set from the heroes' heights. It may need a small adjustment in the
  headset (`--scale`; the characters follow it).
- **Real floor:** the room's floor is set 1.65 m under your eye when you enter, because the game's floor is not read.
- **The game keeps running.** Spider-Man stands still, but enemies nearby can still reach him. Enter when the
  area is quiet.
- **Cost:** the room is about 138,000 triangles drawn unlit, plus 54 texture layers (the model texture array holds
  up to 256). The characters add 2 layers (about 11 MB with mips) and about 118,000 stored triangles. Only one frame
  per character is drawn: about 14,000 triangles on a visit (the sink allows 30,000). All of it is loaded only
  when `ua_locations` is on and the place has been converted.
- **Animation** is a few whole frames of each idle loop, blended. There are no bones at run time, so the movement
  is softer than the game's.
- **Voices** play through the music player's audio path (Windows Media Foundation and WASAPI), one clip at a time.
  They have not been heard in the headset yet.
- **Story state:** conversations that depend on story progress (the laptop, Hank Pym's helmet, the first-visit
  briefings) are left out, because the hub cannot know your progress.
