# HERO VR changelog

HERO VR - Made by Agent Mulder. Full history (back to Spidy): [docs/CHANGELOG.md](docs/CHANGELOG.md).

## hero-0.0.78-cut15-diag - prop skip log, Sandman punches, boss Y card (local branch, not built)

- **PROP SKIP log (first person and mirror apart, at most every 10 s each):** every hero prop model (pistol, pump,
  shield, batons, claws, grapple gun) appendHero007 did not draw, counted by why: model missing, sink full
  (modelSinkBudget), bad place, size check, out of range, over cap, other - with the model sink's fill. OURS's failure
  (oursFault, which clears every hero prop) says so too. Logging only: the code does not show the sink overflowing
  (the props are drawn before the suit in first person; cut14's whole frame is far under modelSinkBudget).
- **SANDMAN PUNCHES (CONFIRMED cause in the public 0.0.77 log):** the BUSY HANDS lines read "enemies 0, boss near 0,
  freed 0" all fight, with 100 "hand holding or carrying" rejects and 1 X strike: the hand whose swing web held never
  punched, because cut7's exemption needs an enemy listed (30 m) or the boss within 40 m and the giant's record is
  84-130 m off. A web-held hand now punches while a living boss is read within 200 m too (live.txt punch_web, as before).
- **BOSS Y CARD (CANDIDATE fix; CONFIRMED causes in the public 0.0.77 report):** the Y zip to Sandman never got a card
  because (1) the Y card's only source, .?AVWebStrikePrompt@@, was never seen in any of the report's 44 sessions, and
  (2) object cards stop at the scan's 60 m pick and the kind's 30 m reach, while Sandman was 84-130 m off all fight.
  The game's own .?AVWebStrikeTarget@@ (in that report's RTTI) is now a Y kind too; one on the boss (his record, or
  within 6 m of him) gets the small Y card 2.5 m over him out to 200 m, before the other cards. live.txt boss_y_card
  (on by default). "HERO VR 0.0.78 BOSS Y CARD" console lines every 10 s while a boss is read prove or refute it.
  Small cards only - no 2D projection of the game. Waypoint / beacon: unchanged, still waiting on a log with CUT9
  ANCHOR / CUT10 GUI2 POI lines (the 0.0.77 report predates them).

## hero-0.0.78-cut14 - back to the cut10 renderer and the old Punisher suit; pistol and pump only (TEST build, private)

From the player's cut13 report (van, black headset, holo portraits, "still look like spiderman"):
- **PBR MATERIALS reverted (d8c806d, eba4641) and cut12's HQ layer order / 24 -> 32 cap (a823c99):** the models are
  drawn by the cut10 renderer again (44-byte vertex, base colour layers at 1024, the per-vertex light). cut13's root
  signature fix is kept on the pre-PBR roots: each root counts its own parameters (std::size), with tests that the
  model root is 2 parameters (b0 16 constants, t0 at heap 0, 3 descriptors), the occluded root 4 (b1 20 constants as
  its draw sets) and the art root 2 (CLAMP, MaxLOD 4); the vertex layout test checks 44 bytes, TEXCOORD at 28 and no
  TANGENT. cut13's session-after-prep order and its "MODELS: prepared in N s" line are kept.
- **Old Punisher suit (sha256 e7171bf7, 23,747 triangles, the cut8 suit):** shipped again instead of the 80k HQ suit.
  Its right drop-leg holster was measured (front and side renders of the glb): the pistol's leg holster moved from
  .85 / .24 / -.06 (the HQ suit's) to below .80, side .285, ahead -.07 so the gun sits in the slab, not against the
  thigh. It models no left-thigh holster, so with the arsenal OFF its pistols get cut8's drop-leg holsters again
  (ownThighHolsters is the HQ suit's only: Suit_punisher_1 with 40,000 triangles or more).
- **Pistol and pump re-converted without PBR maps (same geometry and points as cut12):** base colour only at 1024,
  no normal / ORM maps, no TANGENT (tools/models078/convert_guns_078.py). Credits unchanged: "BERETTA M9 W/
  SUPRESSOR" by drcrazzie and "Remington 870" by White-Horse, both CC BY 4.0.
- **Sniper and rifle slots off cleanly (68-73):** not loaded, not counted ("MODELS: X of 67"), never MODEL MISSING;
  the arsenal is the pistol (right leg) and the pump (back middle) only (punisher078::shipped).
- **Retired live.txt keys:** model_pbr and model_textures_hq from cut11-13's live.txt are read and ignored.
- **IMMERSIVE CUTSCENES ON BY DEFAULT, for everyone:** with no live.txt `cutscenes` line the game's scenes are
  immersive from the first frame (cutscene077::defaultMode; the console says "HERO VR 0.0.78 CUTSCENES IMMERSIVE").
  Once, on the first 0.0.78 start (run_game_vr.py before VR starts, and the launcher), an existing live.txt's
  `cutscenes = flat` or `3d` from an older build is set to immersive and a marker line is added; a value picked after
  that stays. New launcher setting **CUTSCENES** (COMFORT): Flat / 3D / Immersive, default Immersive, written to
  live.txt's `cutscenes` (with the marker, so the move never resets it).
- **The main menu stays flat:** cut9's TITLE FLAT is unchanged; game_xr.cpp's two per-frame cutscene decisions moved
  word for word into cutscene077::menuScreenFrame / immersiveFrame, and tests/cut14_tests.cpp replays the player's
  reports 174648 (cut13b) and 025037 (cut8) through them with immersive from frame 0: the boot, the title and its
  menus, the pause menu, hint cards and pre-rendered videos are the flat VR screen on every frame; the game's scenes
  are still immersive.
- **Test marker:** HERO-VR-TEST-BUILD.txt is key=value again (version, build, commit) with `sha256 <file>=<hash>`
  lines, so the console's MIXED INSTALL check reads it.

## hero-0.0.78-cut12 - PUNISHER ARSENAL: the suppressed pistol and the pump shotgun (TEST build, private)

From the player's models ("just get the models", "not flat textures, it needs to look realistic in game"); this build
ships ONLY these two - the sniper, rifle and red dot are not converted and their slots 68-73 stay as they were.
- **Suppressed pistol (slots 65-67, Models/Punisher078/Pistol, /Slide, /Mag; CONFIRMED offline):** "BERETTA M9 W/
  SUPRESSOR" by drcrazzie, CC BY 4.0. Real scale, grip at the origin, -z down the barrel; frame 17,258, slide 5,437,
  magazine 1,570 triangles; base / normal / ORM at 2048 with tangents. The magazine body and round are built by HERO VR
  around the model's floor plate.
- **Pump shotgun (slots 62-64, Models/Punisher078/Pump, /Forend, /Shell; CONFIRMED offline):** "Remington 870" by
  White-Horse, CC BY 4.0. Scaled 1.12 mm a unit (48 cm; CANDIDATE), grip at the origin; body 14,540, forend (with the
  bolt) 3,940, shell 768 (built by HERO VR) triangles; PBR maps at 2048 with tangents.
- **Points measured on the shipped models** (punisher078::pointsOf): muzzle, ejection port, magazine well, forend,
  rack, sight eye and the rack travel (pistol slide 3.5 cm, pump forend 6.9 cm). Sniper and rifle points unchanged.
- **HQ 2048 layers handed out arsenal first** (mesh_model::textureOrder: arsenal 62-73, then Suit_punisher_1, then the
  rest) and the HQ cap 24 -> 32 layers, so the guns get their full-detail maps (CONFIRMED in tests/model_pbr_tests.cpp;
  in game CANDIDATE: with VRAM unknown the cap is still 16 and the hammers / shield may get the 1024 copies).
- Converter and offline renders: tools/models078/convert_guns_078.py (+ dump_gun_078.py in Blender),
  render_guns_078.py, render_holster_gun_078.py. Credits: launcher, CREDITS.md, THIRD-PARTY-MODELS.md.

## hero-0.0.78-pbr - PBR MATERIALS: the mod's models lit per pixel with normal / ORM maps (TEST build, private)

From the player's ask ("not flat textures, it needs to look realistic in game"):
- **Per-pixel PBR on the mod's own models (CONFIRMED in code and offline; in game CANDIDATE):** suits, the arsenal,
  props and vehicles now carry a 20-byte surface in each vertex (normal and tangent oct, the map layers, the albedo
  tint, roughness / metal / normal scale / occlusion; the vertex 44 -> 64 B). The model pixel shaders read the
  tangent-space normal map (glTF TANGENT, else MikkTSpace-like generated, else the pixel's own uv frame) and the ORM
  map, and light with GGX specular plus a sky reflection from cut10's world light (sun direction and colour, sky, the
  screen-space sun shadow). include/spidy/model_pbr.hpp is the CPU copy of the same math (tests/model_pbr_tests.cpp).
- **HQ 2048 layers** for Suit_punisher_1, arsenal 62-73, hammers 11/15/16/17 and shield 9: base and normal maps at
  2048 when VRAM allows (the most: min(24, (free VRAM - 1.5 GB) / 22.4 MB)); else the 1024 layers.
- **Fallback:** a model without maps is a rough dielectric lit per pixel by its vertex normal; live.txt model_pbr 1 OFF
  gives the cut10 look. A model with maps but no tangents gets generated ones.
- **Punisher suit maps:** tools/suits/convert_suit.py now ships the baked normals, an ORM (cavity occlusion, the
  source roughness and metal) and TANGENT in the glb (about 20 MB, was 5 MB; not in git). The base colour is the raw
  texture: the cavity darkening moved into the ORM's occlusion, so with model_pbr OFF the suit is a little flatter than
  cut10 (CANDIDATE trade-off).
- **Offline previews:** previews/punisher078_pbr_*_offline.png, from tools/models078/pbr_preview.cpp (a software render
  of the emitted vertices through model_pbr::shade under sample lights, not an in-game capture).

## hero-0.0.78-cut10 - Punisher arsenal and world light merged, the leg holster measured, the game's own POI manager found (TEST build)

- **Merged:** hero-0.0.77-cutfix9-weapons (PUNISHER ARSENAL + GUN BLOOD) and hero-0.0.77-cutfix9-light (WORLD LIGHT /
  WORLD SHADOW). The 80k HQ Punisher suit is drawn through the same suit path as every suit, so it gets the world light
  (CONFIRMED in code; in game CANDIDATE).
- **Leg holster grab zone (CONFIRMED against the mesh):** measured on the drawn HQ suit's right-thigh holster, the grab
  point moved from below .735 / side .165 (the belt line) to below .85 / side .24 / ahead -.06. The suit's left thigh is
  a pouch panel, not a holster. The arsenal play test now draws from these constants.
- **stereo_taa:** the retired eye-TAA live.txt switch has a neutral name; the old key is still read.
- **The game's own world prompts / waypoint / beacon (found, not yet in VR):** static RE of 2.810.0.0 shows SM2's world
  UI is GUI2 (Coherent Gameface), not Scaleform. The game's POIView is one static object (image+0xb0f4588, vtable
  0x53cf538) and its POI manager sits inside it at +0x730 (vtable 0x53cf408), holding the per-POI icon records
  (stride 0xc4, count at +0x64) - CONFIRMED by disassembly. The world-to-screen projection those elements use is not
  yet found, so nothing is reprojected per eye yet. New read-only CUT10 GUI2 POI lines (only when both tables match,
  200 a session) log the records and any world place in them, so a log taken with the Y showing on Sandman proves
  which record is the game's Y prompt. Nothing is drawn.

## hero-0.0.78-cut9 - the title flat from the first frame, the beacon's range, the calibration look, no ghost screen (TEST build)

From the player's cut8 reports ("prompts and main menu are top priority for 78"):
- **Title and main menu flat from the very first frame (CONFIRMED by replay):** at boot the menu screen's "no screen"
  reset ran before any play and cleared the front end, so the title's record went immersive until a pause press.
  The reset now runs only in VR gameplay; tests/cut9_tests.cpp replays report 025037 (old behaviour reproduces the bug,
  the new one stays flat).
- **Mission beacon only shown close (far plane and reach ruled out, CONFIRMED; fix CANDIDATE):** the widened game view
  was drawn at a level of detail about 2.9x lower than the eyes; it is now lifted to the eyes' scale (shared_lod HIGHER),
  logged as GAME VIEW LOD.
- **Y zip on Sandman / world prompts (not proven):** the cut8 log cannot show the game's anchor for that prompt. New
  CUT9 ANCHOR lines list every object within 4 m of the boss (class, slot, offset); a log taken while the Y shows on
  Sandman proves the anchor.
- **Calibration screen (cause CONFIRMED by code):** the flat blue BODY CALIBRATION plate is the TECH style's panel,
  drawn only when UI STYLE = TECH (the last item of the SPIDY VR tab). No calibration code changed between 0.0.76.1 and
  0.0.77, so 0.0.77 shows the same. Now calib_look = holo (the default) shows the normal holo checklist in either UI
  STYLE; B still skips. It returns each session because the test launcher never saves the calibration or the skip
  (unchanged; its T-POSE PROMPT option stops it). CUT9 CALIBRATION START / END lines log the look and the seconds shown.
- **Ghost 2D screen in immersive cutscenes (code path CONFIRMED):** 18-23 immersive frames a session had no stereo pair
  and fell through to the flat screen. Such a frame is now no layer (the flat screen only after 1.5 s with no pair, so
  VR never goes black), and the QTE card shows only once a QTE's freeze has held 250 ms. CUT9 NO GHOST SCREEN line.
- **MJ's run and vault by default (CANDIDATE, untested in the headset):** the game asks for MJ's run itself with a
  live RT prompt ("Hold R2"); from that prompt the stick run (story_sprint) is on for the segment, sent as the game's
  own RT, until the game shows a web on the player (Spider-Man swinging), so Spider-Man's controls are unchanged.
  live.txt story_sprint = on still turns it on everywhere. CUT9 STORY RUN lines count the starts and ends.
- **Sounds:** the shield's hit (thrown and bash) and WOLVERINE's stab and two slashes are the player's own WAVs, the
  silence before each trimmed (the transient 1 ms in, no delay added); every hero weapon sound is leveled to -20 LUFS
  max momentary (none past a -1 dBFS peak; the player's guns and THOR's lightning were -3 to +0.6 LUFS: "incredibly
  loud"). The game's own punches and gunfire are in its audio banks and not measured (CANDIDATE).
- **WOLVERINE stab or slash, and more blood:** a claw hit whose move is mostly along the blades (within about 45
  degrees) plays the stab, anything else a slash (two takes); each claw hit throws 160 drops (EXTREME; NORMAL 54) and
  bigger pools, the blood caps doubled.
- **The launcher's What's New:** the update offer and the main screen show what changed in each version
  (version.json whats_new_history, from the release notes).
- **The owner's own HQ PUNISHER (SKULL, Models/Suits/Suit_punisher_1, replaces build 9's VEST):** the 592,596
  triangle model brought to 79,999 (its own cap raised to suitHqTriangles 80,000; every other suit keeps its cap) on the
  same skeleton, the right way up and to scale, with its own drop-leg holsters (the pistols sit in them; the built-in
  holster is not drawn over his). Detail kept: a normal map baked from the 592k source onto the 80k mesh, combined
  with the model's own normal map (texels that disagree with the source's normal by more than about 37 degrees are the
  model's own: 88.8% come from the bake), and its 4K base colour kept. HONEST LIMIT: HERO VR's model shader is base
  colour times a per-vertex colour - no normal, roughness or metallic map at draw time - so the normal detail is baked
  into the colour as shading (cavity), roughness and metallic are not used, and the 4K maps are drawn through 1024 px
  layers (4 atlas quadrants, about 2048 px effective). Lit close-ups of the source and the shipped mesh (head, torso,
  holster) match closely; frame time in the headset is not measured (CANDIDATE).
- **No candy-wrapper wrist (WRIST TWIST, CONFIRMED by apps/wrist_twist_preview on the shipped mesh):** the
  forearm's roll is spread along a twist chain instead of all at the wrist; the forearm's cross-section by the
  wrist stays 89-97% of rest at rolls of -90, 0, +90 and 180 degrees on both arms (it was 49-53% at -90 without it).
  On for the new PUNISHER only; the other heroes' suits were measured but are left unchanged (CANDIDATE).
- **Seated in a vehicle: the eye at a natural reach of the wheel or bars (CONFIRMED by tests, CANDIDATE in the headset):**
  "even with 'seated' the view is far too far back from the steering wheel ... and from the motorcycle handlebars".
  ROOT CAUSE: every vehicle's seat eye was an upright head over the seat or hips with no reach: 0.65-0.81 m from the
  wheel's 9-and-3 grips or the bar grips (van 0.70, 1989 0.66, ODAMI 0.70, FOREVER 0.65, FOREVER 2 0.69, Cap's bike
  0.81, Wolverine's bike 0.79 m). Seated mode did apply the offset in all three axes (AUTO SIT; HEIGHT ONLY the height),
  the vehicle's yaw was handled, and the model scale was right; the level part of the move was also clamped at 0.5 m
  twice, so a short boarding carry left him further back. Now each car's eye is slid forward at its height to 0.58 m
  from the grips (van 0.18 m, 1989 0.09, ODAMI 0.16, FOREVER 0.085, FOREVER 2 0.145), head room re-measured on the
  shipped models (0.13 m or more to the body and the roof); on the bikes the rider leans forward about his hip (Cap 23,
  Wolverine 25 degrees) to 0.58 m. Not nearer: from 0.56 m the van's dash screen lost its lower left corner behind the
  wheel's rim and was seen 56 degrees off; from 0.58 m that corner passes 1.3 cm outside the rim's line (VAN SCREEN test). The seat move's level clamp is 1 m. tests/core_tests SEAT REACH checks every one.

## 0.0.77 launcher hotfix (tag v0.0.77.1) - special thanks on the main screen

Launcher 0.0.77.2, build 20; the mod is unchanged (VERSION 0.0.77, PORT-VERSION Update 39.10). The main screen shows a
small SPECIAL THANKS TO: block under the subtitle (immersive cutscenes; voice acting and testing); the Credits sheet's
cutscene credit line moved there, so it is not doubled. version.json carries "launcher_build": 20, which offers it once to
0.0.77 players (the installed build is 20 afterwards, so no loop).

## 0.0.77 (tag v0.0.77) - cutscenes, story moves, Sandman, the flat front end (EXPERIMENTAL release)

The cut8 build (f686fe4) released with the stamps moved: VERSION 0.0.77, PORT-VERSION Update 39.10, launcher 0.0.77.1
build 19 (HERO_TEST_BUILD stays OFF). Cutscenes flat / 3d / immersive (live.txt `cutscenes`, flat the default), story
moves and the phone's STORY MOVES cheat sheet, Sandman, boss damage read for every hero, the flat front end, the wrist
minimap (in progress), the ledge fix. Known issues: the game's Y zip prompt and the objective waypoint do not show in
VR yet. Every launcher on 0.0.76 or older offers it once; after the update the versions match, so it is not offered
again. Still EXPERIMENTAL, 0.0.63 the most recent STABLE build; Revert unchanged. The launcher's Credits page carries the
immersive cutscenes credit.

## hero-0.0.77-cutscene (8) - Sandman's HP read, a log for every throw, the objective marker's source (TEST build)

From the player's cut7 report ("when you throw the debris at sandman if it hits him it is supposed to DAMAGE him";
"BUILD THE CORRECT IN GAME OBJECTIVE WAYPOINTS THAT THE GAME ALREADY USES JUST MAKE IT APPEAR IN VR DONT MAKE YOUR
OWN DONT GUESS"):
- **Sandman's HP (cause CANDIDATE, fix CANDIDATE):** every BOSS DAMAGE line in every log read "hp -1 / 0": the read
  takes a record's health only from a component with BotHealth's exact vtable (0x531b0a0), which the thugs have
  (0.0.5's DAMAGE PROBE read them) but Sandman's record never showed. The scan now also takes any component whose
  class derives from .?AVBotHealth@@ (by RTTI), and a "CUT8 BOSS HP" line says where his HP came from (none /
  BotHealth / a derived class / only a Health-named class that is not BotHealth, read for the log only) and the
  class's name. The headset confirms it: that line shows a real hp.
- **Every throw logged (live.txt throw_watch, 5598, on by default):** each LB + RB throw writes a "CUT8 THROW WATCH"
  line 2 s later: the boss's HP as the throw starts and at 0.5 s, 1 s and 2 s (and the lowest), his distance before
  and after, the left stick sent the game that frame and the throw aim (story_throw_aim's mode and degrees).
- **The game's objective marker (source NOT FOUND, nothing drawn):** cut7's POI DUMP showed the game's GUI2 POI
  components hold no world place of their own, but words of the registry handle's form. "CUT8 POI TARGET" lines now
  resolve those words through the game's registry and log the class, world place and distance of what each points
  to. No marker is drawn from it: it is the game's marker's source only once a line's distance matches the distance
  the flat HUD shows by the marker and follows it. The waypoints default stays off.

## hero-0.0.77-cutscene (7) - the wrist minimap, off the ledge, Sandman, a flat front end, FPS (TEST build, 0.0.77 RC)

From the player's report on cut6 ("left wrist doesnt show map, i can move after cutscene just not off that ledge ...
got like one hit on him and couldnt damage him anymore ... keep the main menu 2d flat ... remove the text line on
immersive cam ... still bad FPS"), his video and the boot intros' picture a minute behind their sound:
- **Wrist minimap (cause CONFIRMED, fix CANDIDATE):** cut6 cropped the presented frame (WRIST MAP: "frame 1280x720,
  crop 0.016,0.703-0.172,0.981"), but in VR play that frame is the game view with the HUD moved onto the VR panel:
  the brick-coloured panel was the frame's corner, not the minimap. The map now comes from the game's own HUD panel
  texture (live.txt wrist_map_source = frame: cut6's). Left hand to the right wrist + left trigger shows it there;
  right hand to the left wrist + right trigger stays the wrist hologram's, and while the hologram is open the
  minimap shows over that wrist too (wrist_map_holo = off: not there). "NO MINIMAP: HUD TEXTURE NOT FOUND" says so.
- **Off the ledge (cause CONFIRMED, fix CANDIDATE):** every cut6 CRAWL FEED line read "surface 3" (the game's floor
  crawl: a ledge, a roof's edge) and every JUMP line "grounded 0 owned 0 webs 00": the leave-the-crawl RT + A went
  out only on the game's wall crawl (surface 2), so A on the ledge was the crawl's own hop along it and no web ever
  held there. Now A, the web gesture (the web button going down) or a web that holds leaves the floor crawl with RT +
  A (live.txt ledge_jump = game: cut6's). CUT7 LEDGE lines. The horizon stays world-up on walls (live.txt wall_view =
  turned: cut6's view turned onto the wall).
- **Sandman (cause CONFIRMED, fix CANDIDATE):** the fight sent 2 X strikes all session against 68 "hand holding or
  carrying" rejects (and 128 throws, 158 web balls): a hand whose swing web held - the fist is the grip that webs -
  punched nothing. With an enemy listed or the boss within 40 m that hand punches (live.txt punch_web = off: cut6's).
  The BOSS DAMAGE line read "boss none hp -1/0" because the boss read was skipped for SPIDER-MAN (CONFIRMED in the
  code); it is read for every hero now (his blows stay the game's own). CUT7 BUSY HANDS lines name what holds a hand.
- **Front end flat:** the title, the front end and the pause menus are always the flat VR screen; immersive only for
  in-game scenes (cutscene077::MenuScreen, timed sequence in tests/cutscene077_tests.cpp).
- **No text line on the immersive view:** why a scene is flat goes to the log (live.txt cut_note = on: cut6's line);
  the small card shows only while a QTE is live.
- **FPS:** the mod's own paths measured under 0.3 ms a second at 90 Hz (the game GPU-bound near 37 fps). The eyes'
  depth copy runs only while the see-through car models draw (live.txt see_through_depth = always: cut6's) and the
  periodic diagnostics are every 30 s, the FRAME line every 10 s (diag_logs = full: cut6's).
- **Boot intros (CANDIDATE):** CUT7 BOOT SCREEN lines (every 2 s for 90 s: game presents and screen copies a second,
  skips, the copy's age) measure the picture's lag on the VR screen.

## hero-0.0.77-cutscene (6) - the swing starts after the title, leave the ledge, no stick menu (TEST build, 0.0.77 RC)

From the player's report on cut5 ("Still stuck in that spot on that save. Still cant swing or even do a charged jump
after getting out of immersive cutscene"), session 234713:
- **Native swing start (CONFIRMED):** the swing was started on the title screen's one VR-play frame (sample 402),
  that player record was gone a frame later, the start was refused 1002 (mover not live) and stayed off for the
  whole session: no RT web swing, no charged jump, the swing's ground 0 in every JUMP line, and none of what starts
  with the swing (the game's wall crawl and wall interactions). The start now waits for one player record held in VR
  play for 0.5 s, tries a "not now" refusal (1000/1002/1004) again a second later or at once on the next player, and
  only a refusal of its bytes or hooks turns it off (as before). A swing that cannot follow a new player no longer
  drops him: it stops and starts again on him. CUT6 SWING START / CUT6 SWING lines.
- **The ledge save (CANDIDATE):** both stuck sessions (cut4, cut5) ran without the swing; both that left the spot
  ran with it. It now starts there (replay: 0.5 s after VR play begins on that save).
- **Menu screen:** the left stick is no menu signal any more (it ended two scenes as "menu"); the title, no player
  record or the menu button, and only while the game's screen is up - never in VR gameplay.
- **Charged jump:** unchanged from 0.0.76 (the swing's ground; without the swing A is the game's plain jump).
- **Replay test:** tests/cut6_replay_tests.cpp runs the 234713 session through the real decisions (swing start,
  menu screen, pad mapping, charged jump) and fails the build if he could not swing or leave the ledge.

## hero-0.0.77-cutscene (5) - free movement after scenes, cine prompts, the wrist minimap (TEST build, 0.0.77 RC)

From the player's report on cut4: after the new-game scene he could not steer his walk or his swing (zips, veers),
the ledge save stood him still, the menu fix showed the whole flat game in the immersive scene, and he wants the
map / objective.
- **Story sprint (CONFIRMED blind ground):** its "grounded" was only "the grip swing does not own him", true in the
  air - the RT it held on was the game's own R2 web swing (camera-steered webs, the zips and veers). Off by default
  again (as 0.0.76; live.txt story_sprint = on to try it) and, when on, the game mover's own ground flag. The other
  synthetic holds were audited: none is blind. CUT5 STORY SPRINT and CUT5 MOVE lines log what the game got.
- **Stuck watch gone:** cutfix3's watch reset the game's screen while he stood still; gameplay after a scene is the
  same path as gameplay without one.
- **Cine prompts:** the scene's own Y offers (Where cine kinds) get a Y card for 12 s once in reach and Y routed to
  the game the whole time; CUT5 CINE PROMPT lines count each offer and press. The stuck ledge save: CANDIDATE.
- **Menu:** a menu over a scene goes to the normal VR screen (as cutscenes = flat), never the whole flat frame in the
  immersive world (CUT5 MENU SCREEN lines).
- **Waypoint diamond off** by default: the game's objective POI source is still not found (POI DUMP lines stay).
- **Right-wrist minimap (CANDIDATE, new):** left hand to the right wrist plus the left trigger shows or hides the
  game's own HUD minimap on a panel over the right wrist. It is cropped on the GPU from the game's presented 2D frame
  (bottom-left HUD corner, by the frame's height and live.txt wrist_map_hud) - not from the eye views, never read
  back. In a menu, a cutscene or without the hero the game hides it and the wrist says MINIMAP NOT ON SCREEN. live.txt
  wrist_map (on, off), wrist_map_size, wrist_map_at, wrist_map_hud; WRIST MAP lines. Costs one screen copy a frame
  only while shown.

## hero-0.0.77-cutscene (4) - waypoint findings, POI dump, HUD panel answer (TEST build)

From the player's report on cut3: immersive mode worked; the waypoint never appeared; he asks for the game's HUD,
smaller, or the pause map on the holo.
- **Waypoint (CONFIRMED from his log):** 20 scans, every one "no objective POI found". The scan saw 7-11 game POI
  components each time (POIGame 7, PointOfInterest 4, SyncedPOIObject 1) but none had a POI type the reader could find
  (typed 0): the type is not a pointer in the component's first 64 words. So the diamond's source does not work and
  it shows nothing (no guesses). It stays as is (invisible) only to drive the new dump.
- **POI DUMP (new diagnostic):** each POI component the scan meets (up to 40 a session) is written to the console as
  `HERO VR 0.0.77 POI DUMP n ...` lines: its class, position, distance, and the RTTI class / small integer in each of
  its first 64 words (one level deeper too). Lines are split under 240 characters (the console cuts at 256).
  Scan with a mission active and with his own map waypoint set, then send the log.
- **Frame-rate drops (his report):** from his logs, gameplay ran ~31 fps (eye render ~21 ms) against ~46 fps
  (eye ~16 ms) in the 0.0.76 visual tests at the same 2688x2880, while the immersive cutscene in the same session
  ran ~46 fps (eye 16 ms). CONFIRMED: the drop was already in cut2 (33.7 fps median), so it is not cut3's POI scan
  or cutfix2's camera sample; it is gameplay-only and GPU-side (the eye render time rose). The scan thread's scans
  stay under 25 ms (it backs off otherwise). Cause CANDIDATE, not proven: heavier gameplay rendering since story8.
  New: `HERO VR 0.0.77 FRAME (VR loop)` once a second - average and worst loop frame, and ms/s + peak for the cards +
  waypoint block, the camera sample and the POI dump - so the next log shows which path, if any, costs time.
- **The game's HUD smaller in VR: not in this build.** On SM2 the game draws its HUD (cohtml) straight into the final
  frame; the eye images never have it, and the mod has no hook on a HUD-only texture yet (the SM1 HUD panel is not
  ported). The only capture that exists is the whole frame, which is ruled out. A probe for a HUD-only render target
  is the next step.
- **Pause map on the holo: not in this build.** It needs the objective's position (the same missing source) or a
  capture of the map screen (whole frame again).

## hero-0.0.77-cutscene (3) - the WAYPOINT is the game's own objective, shown after a scan (TEST build)

From the player's report on cut2: the diamond showed but did not lead to his missions, and should be the game's own.
- **Root cause (CONFIRMED from his log):** no POI action was ever found (POI 0); the diamond fell back to the nearest
  mission start (236-570 m off) or an interactable - guesses, not his objective.
- **Fix:** those guesses are gone. The only source is the game's own POI markers (the ones its HUD draws), read with
  their POI type: POITypeObjective, Quest, ZipToObjective or his own map waypoint (CustomWaypoint). None found:
  nothing shown. (CANDIDATE: the classes are CONFIRMED in the exe; the log now counts the POI types it finds.)
- **Only after a scan:** hidden by default; each scan (hand to temple + trigger) shows it for about 8 s, fading out.
  live.txt `waypoints`: `scan` (default), `always`, `off`. The log notes each scan's show.

## hero-0.0.77-cutscene (2) - INTERACT CARDS and the yellow WAYPOINT in VR (TEST build, not released)

From the player's report on story (9): the truck to move for the firefighters, the water pipe to break and the things
to grab for a mission had no prompt in VR, and the game's yellow objective waypoint is not shown in VR.
- **Root cause (CONFIRMED from his logs and the code):** PROMPT CARDS knew only the throwable, web strike and QTE
  classes (0 object cards shown in his sessions); the generic interactables were no kind. The waypoint is the game's
  cohtml HUD, which VR does not show, and the mod drew none.
- **INTERACT CARDS** (CANDIDATE; the classes CONFIRMED in Spider-Man2.exe's RTTI): world-anchored cards over
  VehiclePushable ("GAME: LB+RB HOLD + LEFT STICK BACK / VR: HOLD RIGHT STICK CLICK + LEFT STICK BACK", 20 m),
  WaterPipe ("LB+RB HOLD / HOLD RIGHT STICK CLICK", 20 m), EnviroInteractBase (LB+RB, 12 m), InteractLocationOffer,
  PickupCarryableOffer and HoldOffer ("HOLD Y", 8-10 m), InWorldInteractOffer and any other ProximityOffer
  ("Y (HOLD Y: WALKS YOU TO IT)", 6-10 m). The most derived class wins. An offer on the hero himself is never a card.
  The throwables' line is now "HOLD RIGHT STICK CLICK / TWO-HAND YANK" (what sends the game's LB+RB).
- **The holds reach the game:** with a pull card up, the right stick click's hold is let through while a swing owns
  him too (pullCardGate); with a Y card up, Y the same (yCardGate). Not while flying, the X ability or a named R3.
- **WAYPOINT** (CANDIDATE, live.txt `waypoints`: on (default) / objective / off, SpidyWaypoints 5580;
  include/spidy/story077g.hpp, src/waypoint_mark.cpp): a yellow diamond the mod draws over the objective's place with
  "OBJECTIVE 123 M", over the game's picture (walls do not hide it), the same size in view; out of view (38 degrees)
  a yellow arrow at the view's edge. The place: the newest AddPOIAction / AddInteractPOIAction with a place, else the
  nearest truck / pipe / hold offer / InteractLocation 3-150 m away, else (on only) the nearest MissionLocationProxy
  ("MISSION 1.2 KM"; its place CONFIRMED readable in his world trace). Never a picture of the game.
- Diagnostics: first seen per kind with the nearest place; every 10 s the kinds with their nearest distance, the pull /
  Y card frames (click held, LB+RB sent, let through a swing, blocked by fly / chord / named R3 / X), and the waypoint
  (frames, shown, drawn, edge arrow, source, class, place, metres); a note each time the waypoint changes (200 max).

## hero-0.0.77-cutscene (1) - CUTSCENES: 3D and immersive scenes, opt-in, with story (9)'s heal and crawl (TEST build, not released)

A separate test build on hero-0.0.77-story (9) (5ec540e: HEAL ANYWHERE and the floor crawl's left stick, both kept:
an immersive scene never changes the pad's mapping, so SCREEN HEAL's D-pad down and the crawl's raw stick work as on
story (9)). New live.txt `cutscenes` (SpidyCutscenes 5571; include/spidy/cutscene077.hpp):
- **`flat` (the default)**: the game's frame on the virtual screen, exactly as before. Default because both new
  modes depend on CANDIDATE values only a run on Spider-Man 2 can confirm; flat makes nothing worse than today.
- **`3d` (GAME SCREEN 3D)**: the game camera's stereo pair on the virtual screen. Spider-Man 2 has no HUD panel hook,
  so the panel signal is the game frame's subtitle strip landing on the pair: the pair waits 250 ms for its first
  image, keeps its 200 ms panel rule, and gives up stereo for that scene after 1 s with no strip. Never asked for in
  gameplay or VIEW: GAME SCREEN.
- **`immersive`**: native eye mode 4. On the scene's first frame the head is levelled (yaw only); each frame each
  tracked eye, relative to that head, is placed on the game's scene camera (main view base+0xc2ca220, CONFIRMED) with
  the headset's own lens (0x2c75560 / 0x2c76310, CONFIRMED). No hands, webs, markers or body. The Xbox mapping stays.
  While the game clock is frozen (a QTE) and no menu was pressed the scene holds, the whole frame's prompts keyed on
  the see-through panel; the menu button (a pause), or a freeze over 15 s, goes to the screen. Subtitles on the
  panel otherwise. A refused or failed eye command, or a panel fault, turns only that part off for the session
  (never a crash, never the flatscreen fallback).
- CANDIDATE: the subtitle strip's place in the frame (12-88% across, 78-97% down), the 15 s QTE bound, the 1 s menu
  reach, the key's thresholds, the panel's place (about 19 degrees below eye level, 2 m out), mode 4 rendering on SM2 itself.
- Diagnostics: every 10 s while `cutscenes` is not flat, "HERO VR 0.0.77 CUTSCENES <mode>: 3d pairs asked / shown,
  strips set / drawn / missed; immersive frames, starts, commands, refused, faults, shown, waits, eyes placed,
  panels set / drawn / missed, QTE frames, menus", plus any "... OFF (fault)". The run report calls the immersive
  scene's frames `cutscene_immersive`.

## hero-0.0.77-story (9) - HEAL ANYWHERE: the right stick heals on the game's card too; the crawl's stick (TEST build, not released)

From the player's story7 run at the walkthrough's 13:40 (the ceiling falls, the second spider crawls out hurt and the
game wants its heal, the D-pad down). In his log the left stick did crawl him (418 crawl samples at 0.6 m/s and up,
368.8-374.3 s); then the game's Focus Bar card ("PRESS A TO CONTINUE") put the mod on the game screen for 41 s, and
there the right stick was only ever the game's right stick, never its D-pad down (CONFIRMED in code: Mapping::menus).
Now (include/spidy/story077f.hpp, live.txt `screen_heal`, SpidyScreenHeal 5570, on by default):
- **Right stick held down heals on the game screen too while the hero is hurt** (CONFIRMED in code and tests; that it
  is what the card wants is CANDIDATE): the game's D-pad down for as long as it is held, at least 0.3 s. At full
  health a menu's right stick is left alone. A card still closes with A.
- **In VR play a second heal (HealStick)** beside StickPush: a diagonal more down than aside counts, a push that
  wandered aside first is not spent, a push the game names a D-pad way for heals when held down. Not while X holds
  the stick's down for camo, or mid hover flight (CONFIRMED in code and tests).
- **The fallback hint**: hurt below 30% for 2 s with no heal, the headset says "Heal: push the right stick down and
  hold it ... A game card on screen? Press A to close it, then heal", at most every 30 s.
- **The floor crawl gets the left stick** where the swing still owned the player and the walk was left at rest
  (CANDIDATE: his log shows it fed in play; this covers the case it was not). The ceiling crawl is the game's
  scripted state, not the mod's locomotion: with nothing from the walk there, the raw left stick goes to the game.
- Diagnostics: "HERO VR 0.0.77 HEAL: D-pad down out #n (VR play / game screen), health, right stick" and its held
  ms, "HEAL ANYWHERE" when the second way healed, "HEAL HINT shown", and a once-a-second "CRAWL FEED" line on the
  floor crawl (left stick, the game's stick sent, swing owned, surface, body speed, forced frames).
- The VR console line shows story7's X / menu controls (it still said "X: game menu").
- `screen_heal = off` keeps the game screen's right stick the game's own.

## hero-0.0.77-story (8) - X IS THE GAME'S X: the game menu moved to the menu button held (TEST build, not released)

From the player's test of 05e70a2: in the Sandman fight as the second spider, "tap X" to hit him with debris also
opened a game menu, because a VR X tap sent a 0.1 s View. Now (include/spidy/story077e.hpp, live.txt `x_button`,
SpidyXButton 5569, game by default):
- **VR X tap = the game's X (Square)**, as Peter and the second spider (CONFIRMED in code and tests): one short press
  (0.07 s) per tap, sent as X is let go (the ability button has to tell a tap from a hold); fast taps queue (up to
  4) and each is its own press, for "rapidly press X". Hold X + punch / Y / B / A (the LB abilities) are unchanged.
  X held 0.8 s alone: the mod's gadget wheel where it is offered, as before; otherwise the game's X held (it was
  View held). The other heroes keep X as it was.
- **Menu button held about 0.5 s = the View X used to send** (the same 0.1 s View), with a short buzz; that hold
  never pauses. A tap pauses as before, but as you let go (a hold has to be told from a tap). Both sticks held in
  still pause at once; the phone's SWITCH HERO sends its own View hold as before; the phone and the wrist holo
  are unchanged. Whether that View is the same game menu he saw is CANDIDATE (same button, same length).
- `x_button = view` puts 05e70a2's controls back (X tap View, the menu button pauses on press).
- Diagnostics: a note per X tap sent as the game's X (the first 20), a "HERO VR 0.0.77 MENU HOLD" note and console
  line per hold, and a "HERO VR 0.0.77 X BUTTON" console line (at most every 10 s, when a count changed): X taps
  sent as game X, X held as game X, X taps as View, menu taps as pause, menu holds as View.
- The phone's STORY MOVES card reads "X TAP / MENU HOLD - GAME X / VIEW"; the prompt cards say "TAP X / PUNCH" and
  "TAP X FAST / PUNCH FAST".

## hero-0.0.77-story (7) - PROMPT CARDS: no full frame in VR, the prompts as VR cards (TEST build, not released)

From the player's test of b8b33ce: the PROMPT WINDOW put a flat copy of the WHOLE game screen in VR. That is gone.
- **No full frame** (CONFIRMED in code and tests). live.txt `prompt_window` is a debug view now: off (the default)
  or debug_full. In VR play the game's frame is copied only for debug_full (wantScreen), and the window is drawn only
  then. His `prompt_window = off` line, and the old on / large / 0 / 2 lines, are all off. None of them turns the
  cards off. Junk values get a note and are skipped (the default stays: off).
- Correction to (5): the SM2 HUD is cohtml (Coherent GUI2), not Scaleform. Its prompt texts and icons are not read.
- **PROMPT CARDS** (CANDIDATE, live.txt `prompt_cards`: on / off, SpidyPromptCards 5568, on by default;
  include/spidy/story077d.hpp, src/prompt_cards.cpp). Small cards drawn by the mod, never a picture of the game. Each
  names the game's button and the VR control that sends it, e.g. "GAME: LB+RB / VR: BOTH TRIGGERS / YANK",
  "GAME: MASH X / VR: PUNCH FAST", "GAME: Y / VR: Y (WEB STRIKE)".
  - Source: world_trace's scan (read-only, the same guarded reads) keeps the objects of the game's prompt classes
    (RTTI names CONFIRMED in Spider-Man2.exe; that each stands for a prompt on screen is CANDIDATE): WebStrikePrompt,
    GrabAndThrowable, ThrowablePickupPoint, WebSnatchSpecialThrowable, LiftableInteract (object cards);
    ButtonMashInteract and the CineQTEShotController actions, CombatQTEOverlayTrigger, JobEntryCombatQTE,
    QTEPromptAction (screen cards); SimplePrompt, TutorialContextualPrompt and others watched only.
  - Object cards: over the nearest two throwables / web strike targets within reach (12-30 m), world-anchored and
    facing you. Screen card: a QTE / mash object that appeared since VR play began, a small card a bit below the
    centre, for its kind's few seconds. Nothing when none qualifies.
  - Only in VR gameplay (the gameplay mapping, not flat, not the menus); the scan is asked for only then.
  - Diagnostics: a note when each prompt class is first seen, and a "HERO VR 0.0.77 PROMPT CARDS" console line
    every 10 s (frames, scans, object / screen card frames and how many shown, cards drawn, full frame "never",
    each class live / appeared / gone).
  - Not known until a run: whether these objects exist only while the prompt shows (the scan is 1 s, so a card can
    be up to about 1 s late).

## hero-0.0.77-story (5) - PROMPT WINDOW + BOSS DAMAGE (TEST build, not released)

From the player's Sandman test of 718d933 (report game-vr-20261103-150713): no debris highlight and no button
prompts in the headset. His inputs did go out (B 14, RB 10, X 5, RT run 18), but LB + RB 0 and throw yanks 0.
- Why (CONFIRMED in code and in the report): the game's Scaleform HUD draws its prompts, QTE cues and throw
  highlights only into the frame it presents to the monitor, never into the eye images. SM2's HUD and prompt
  hooks are not ported (prompts invokes 0, hud frames 0).
- **PROMPT WINDOW** (CANDIDATE, live.txt `prompt_window`: on / large / off, SpidyPromptWindow 5566, on by
  default). A copy of the game's presented frame, with its own prompts and markers, is shown in a window low in
  both eyes, at infinity (story077c.hpp). The copy is made at Present before the mirror. The mirror, the
  monitor's rate hold and the game view's lens are unchanged (screenForInset). A fault in the window turns only
  the window off ("PROMPT WINDOW off: ..."). A "HERO VR 0.0.77 PROMPT WINDOW" console line is written every 10 s.
- **BOSS DAMAGE** (CANDIDATE, live.txt `boss_damage`: on / off / strong, SpidyBossDamage 5567, on by default).
  Sandman (.?AVSandmanBot@@) was 269 m away in the trace, outside the 30 m enemy list, so no hero's weapon could
  pick him. While a non-Spider-Man hero faces the nearest boss (within 35 degrees, up to 600 m), a point 6 m along
  the look stands in for him in the weapon lists only (after the lock / lunge). THOR's lightning and hammer,
  IRON MAN's blasts and chest beam and the other heroes' blows that hit it go to the boss's own record on the
  sm2_damage path, with no knockback (strong: x4). HULK's thunder clap while facing him is a 60 HP blow.
  Diagnostics: a note when a boss is first seen, and a "HERO VR 0.0.77 BOSS DAMAGE" console line every 10 s (the
  boss, its distance and HP, frames facing, blows queued / sent / not-slot, claps). Whether boss damage moves a
  scripted phase on is not known until a run.

## hero-0.0.77-story (4) - LAUNCH AUDIT: SWITCH HERO is gameplay-only (TEST build, not released)

- Launch/main-menu audit of every 0.0.77 addition. One risk found and fixed (CANDIDATE, not a crash): the
  SWITCH HERO head gesture (a hand still on the head for 1.2 s) and the phone's SWITCH HERO could start the
  switch's View / X / B presses at the main menu, on a loading screen or on a game screen. Both now start only
  in gameplay. A refused start writes "SWITCH HERO: refused, not in gameplay" and counts in STRY refused_busy.
  Nothing else changed.

## hero-0.0.77-story (3) - STORY INPUTS: mashing, sprint, every press counted (TEST build, not released)

- MASH X (CONFIRMED in code): a punch that came while the last punch's X (0.18 s) was still held merged into
  one long press, so "Rapidly Press Square" got one press for many punches. Now each such punch lets X go
  for 0.04 s first, so every punch is a new press (combat_input.hpp punchAgain; counted as mash splits).
- STORY SPRINT (CANDIDATE, live.txt story_sprint, SpidyStorySprint 5565): the left stick pushed all the way on
  the ground for 0.15 s holds the game's RT (its run) until the stick drops under 0.6. VR sent no RT on the
  ground except while crawling. This is for the run-for-your-life and up-the-roof sections.
- STORY INPUTS diagnostics (STRC SpidyStoryInputsData, parse_story_inputs): every press the game is sent is
  counted, with how many were in the air, for B, LB+RB, Y, RB, X, LB+X, Y+B, D-pad down, LT+RT, A, RT held,
  LB+B and LB+Y. The record also keeps the most presses in 1 s for X, RB and LB+RB. Each press writes a
  "HERO VR 0.0.77 INPUT" note (at most one per input per 0.25 s), and the summary gets a line.
- The phone's STORY MOVES card adds the Spider-Arms / symbiote note (the same LB slots), Surge (X held +
  stick click), mashing and the sprint.

## hero-0.0.77-story (2) - STORY MOVES: abilities, aimed throws, dodges anywhere (TEST build, not released)

On top of 081c102 (STORY INPUTS). SM2 only. **Built and unit-tested, not yet tried in the game.** Every press is the
game's own pad button; every timing below is CANDIDATE until a headset run.

- **ABILITIES (Peter, the second spider, symbiote forms).** The game's abilities are LB held + X / Y / B / A, the
  ultimate L3 + R3, camouflage D-pad Up. VR had no way to hold the game's LB with a face button (CONFIRMED in code:
  VR X is View / the gadget wheel). Now **hold VR X**, then: **punch** = LB + X (Venom Punch / Spider Barrage),
  **Y or right stick up** = LB + Y, **B** = LB + B, **A** = LB + A, **right stick click** = ultimate, **right stick
  down** = camo. LB goes out first, the face button 50 ms later, held at least 0.2 s. While X is held nothing else
  of the face buttons, heal, zip or pull goes out, so the punch logic cannot eat it. X tapped alone is still View;
  held 0.8 s alone it is the gadget wheel again. The phone's new **STORY MOVES** card is the cheat sheet.
  live.txt `story_abilities` (on default / off); live.
- **THROW AIM.** While the game's LB + RB throw goes out (and 0.4 s after) the left stick points the throw where you
  **look** (default) or where your **right hand points** (latched at the throw's start), in the game camera's frame;
  your own stick wins. live.txt `story_throw_aim` (look / point / off); live.
- **DODGE.** B never reached the game in the air, on swings or on walls (CONFIRMED in code: the dodge was only read
  while the swing did not own the hero). Now **B dodges at once everywhere** (each press held at least 0.12 s, never
  delayed by the fight logic), a **quick sideways step of your head** dodges that way (near enemies or on a
  spider-sense warning), and **both controllers buzz hard at spider-sense's NOW** (under 0.3 s to the blow) - press B
  then for the perfect dodge. live.txt `story_dodge` (on / off), `head_dodge` (fight / always / off); live.
- **DIAGNOSTICS.** The timeline and console say each ability, each aimed throw and each head dodge. The new STRB record
  (SpidyStoryMovesData) puts the counts in update37.json and one line in the report.

## hero-0.0.77-story - STORY INPUTS: the hero switch and the Sandman web throw (TEST build, not released)

On top of v0.0.76 (792386b). SM2 only. **Built and unit-tested, not yet tried in the game.** Every press below is the
game's own pad button; timings and the PC FNSM button are CANDIDATE until a headset run.

- **SWITCH HERO (Peter / the second spider).** VR never reached the game's own switch: its FNSM app needs View held
  (VR X tapped is a 0.1 s View, held it is the gadget wheel) and then the game's X held, which VR play never sent.
  Now the phone's home screen has a **SWITCH HERO** card, and **one hand laid flat on top of your head, still, for
  1.2 s** (a buzz at 0.5 s, triggers and grips let go) does the same: View held 1 s, then X held 2.2 s, then the
  hero probe (HSWP) watches 3 s for the new hero; none closes the app with B. Your pause (both sticks in) cancels it.
  live.txt `story_swap`: `view` (default), `dpad_left` / `dpad_right` (that shortcut set to Open FNSM App in the
  game's controls), `off`; live.
- **WEB THROW IN THE AIR (Sandman's debris, "LB + RB (Hold) Throw").** The right stick's click (the game's LB + RB)
  now goes out while a web carries you too, and the swing lets the game have you for it; and **both hands held out in
  front (0.3 s) then yanked back to your chest together** holds the game's LB + RB for 1 s, on the ground, on walls
  or in the air, with no web of yours on. The game picks the highlighted object; nothing of the mod's own grab is
  used. live.txt `story_throw` (on default, off = 0.0.76's ground-only pull); live.
- **DIAGNOSTICS.** The timeline and console say each step: SWITCH HERO asked (phone / head), each phase, the result
  (switched / no new hero / cancelled), each throw (yank or click, ground or air). The new STRY record
  (SpidyStoryData) puts the counts in update37.json and one line in the report.
## 0.0.76 in the launcher (tag v0.0.76.1) - the v0.0.76 files, version stamps only

The published v0.0.76 (HV076.zip) still said VERSION 0.0.75, so no launcher could offer it. v0.0.76.1 is the same
code with only the stamps moved: VERSION 0.0.76, PORT-VERSION Update 39.9, launcher 0.0.76.1 build 18. Every
launcher on 0.0.75 or older (0.0.63 included) now offers 0.0.76 once; after the update the versions match, so it is
not offered again. Still EXPERIMENTAL, 0.0.63 the most recent STABLE build; Revert unchanged.

## hero-0.0.76-bikes - a Harley for each hero, Cap's chest-draw grapple, Punisher holo guard (TEST build, not released)

On top of test 27041ea (AO ON + ONE PAGE). SM2 only. **Built and unit-tested, not yet tried in the game.**

- **A HARLEY FOR EACH HERO.** The 0.0.75 sci-fi motorcycle is gone. A short **X** tap summons the hero's own bike
  (driver only): CAPTAIN AMERICA a 1936-47 Knucklehead "EL OHV V-TWIN" (matias_romero, CC BY 4.0, 2.22 m long),
  WOLVERINE a 2016 FXDLS Dyna Low Rider S (everhard, CC BY 4.0, 2.335 m long). Both keep their own textures and
  metallic/roughness. Each one's handlebars, fork and front wheel are a separate part that turns on the bike's own
  steer axis when you turn the grips. Slots: `Models/Vehicle/Motorcycle/Cap` and `.../Wolverine` (each `model.glb`,
  `Handlebars/model.glb`, `layout.json`, `license.txt`); replace a folder's files to swap that hero's bike, or put
  your own bike's files plus an `override.txt` in `Models/Vehicle/Motorcycle` to use it for both heroes. A missing or
  broken file shows MODEL MISSING (no stand-in bike). Converted by `tools/bike076/convert.py`; credits in THIRD-PARTY-MODELS.md and CREDITS.md.
- **DASH SCREEN AND CRIME TRACKER** on each bike: the screen is centred between the bars, upright and facing you, on
  the frame, clear of the bars through their whole turn; the crime tracker sits level just above it.
- **CAPTAIN AMERICA'S GRAPPLE works like the PUNISHER's:** reach to your chest to draw it, then point and pull the
  trigger.
- **PUNISHER DUAL-WIELD:** while either hand holds a gun, or for 2 s after your last shot, bringing your hands
  together no longer opens the wrist hologram (the one-page settings). Otherwise it opens as before; an open
  hologram still closes the same way, and F8 is unchanged.
- **FIX (0d48cf3's report game-vr-20261102-233712): suits, roster pictures and textures are back.** The two bikes'
  images took the model textures to 167 layers; the loader allowed 192 but the overlay refused anything over a
  literal 128, so the whole texture array was dropped ("MODELS: UNTEXTURED"). Suits are only worn, and the holo
  roster's pictures only drawn, with that array ready, so heroes gave weapons but no suit and the roster showed
  initials. The overlay now takes the loader's own limit (one number, 192; about 270 MB at most).
- **FIX: Ambient Occlusion is OFF by default again** (`vr_ao = on` in live.txt turns XeGTAO on, next VR start). Every
  eye anti-aliasing value in the report matched the good 47cd349 runs; the only picture change was Ambient Occlusion
  Off -> XeGTAO, whose noise is meant for the game's TAA, which never runs on the eyes, so it shimmered as worse
  aliasing (and cost about 2 ms of eye GPU time a frame).
- The release zip's top folder is short (`HV076`), so Windows extracts it without hitting the path limit.
- **FIX 2 (e808401's report game-vr-20261103-001914: "no cap grapple no cap bike no wolverine bike").**
  - **CAP'S GRAPPLE, root cause found:** only the hand without the shield (the left, by default) could draw it, and
    a right-hand grab at the chest lands in the left forearm's hologram zone, which swallowed the grip and made the
    trigger toggle the wrist hologram (the report: hologram toggles at 5-9 cm right after PLAY AS CAPTAIN AMERICA,
    no draw at all). Now either hand draws at the chest (the shield arm's too, while the shield is on the back),
    a grip at the chest rig is the grapple's draw rather than the hologram zone's, the drawn gun's trigger is the
    gun's, and the hands-close gesture cannot open the hologram while the grapple is drawn. Every draw, and every
    near-chest grip that drew nothing (with the reason), goes to the prompts log.
  - **BIKES:** the report has no trace of the X press reaching the summon, so the exact gate is not proven. Fixed
    what the code allowed to fail silently: a press of X longer than 0.4 s summoned nothing and said nothing (now a
    tap is up to 1 s, and a longer hold says so on the wrist); a found spot could be dropped on a frame out of play
    (now it waits for a frame the bike can use it); a street check with no answer in 2 s now says so. Each X press
    as CAP or WOLVERINE is logged with where it went (in play or not, the bike ready or not).
  - CAP's hero note now describes him (it said "webs").

## hero-0.0.76-sm1 - melee, web balls, flight (TEST build, not released)

From test 801ce57, then test 32bd648's feedback. **Built and unit-tested, not yet tried in the game.**

- **AMBIENT OCCLUSION ON BY DEFAULT (new live.txt `vr_ao`, on by default, next VR start).** If your Ambient
  Occlusion is Off, VR sets it to XeGTAO while it runs (soft contact shading that gives buildings and streets depth;
  about 1-2 ms a frame, CANDIDATE) and puts yours back when the game closes; `vr_ao = off` leaves yours as set. The
  comfort advice now suggests XeGTAO instead of Off.
- **ONE SETTINGS PAGE on the wrist.** VR SETTINGS and GRAPHICS are one page (it scrolls): RECALIBRATE, FPS, then
  BODY AND VIEW, WEBS, HOLOGRAM, MIRROR, SOUND, GRAPHICS and PERFORMANCE. The test and diagnostic rows (anti-aliasing,
  sharpness, history, supersample, eye AA, trail, TAA jitter, eye test, address finder, foveated debug, eye LOD,
  shared LOD, lens flares, world log, and the web ball / foveated fps / slot test presses) are gone from the
  hologram; the ones with a live.txt key still work from there. Defaults unchanged.

- **SHADOW STABLE (test; new live.txt `vr_shadow_stable`, off by default, live, no restart).** Run 3's picture
  (`vr_aa` ours, `vr_taa_depth` off, `vr_ss` off) with flickering far building shadows. Found (CONFIRMED in the
  game's code): the game's far shadow cache is one object shared by every view, remade each frame by any view the
  game is told is "active"; the left eye is told so, and so is the head's own view. The visual reports REFUTE the
  first idea (an eye getting no far shadows): every eye fit had them. CANDIDATE: the two remakes race each frame and
  the right eye takes whichever came last, so the far shadows jump. With it on the left eye no longer remakes them:
  they are made once a frame from your head's view and shared by both eyes (the same as `far_shadows` 1 for the far
  shadows; the near ones are still fitted for each eye). The game already snaps its shadows to their texels.
- **TEXTURE DETAIL (test; new live.txt `vr_texture_detail`, off by default, next VR start).** More surface detail
  without sharpening: `aniso` gives the game's material textures it left trilinear 16x anisotropic filtering (most
  were already 16x, so a small change at a small cost), `quality` sets Texture Quality Very High while VR runs (your
  setting put back after; more video memory and possibly some streaming hitches), `on` both.

- **SEAM / EDGE TRACE (diagnostics only, read-only; new live.txt `vr_taa_seam`, 0 off the default, live, no
  restart).** The visual-9 test ("doesnt settle", SpidyVR's TAA, `vr_taa_depth` off) had no depth-on logs and the
  earlier trace only ran with `vr_taa_depth` on. With `vr_taa_seam` on the trace also runs on the head-motion path:
  a separate TAA_COUNT + TAA_AGE pipeline variant reads the eyes' depth for its bins only (the history is still moved
  by the head motion, exactly as with it off). Every 10 s, by movement (still / moving / fast), angle (frontal / mid /
  grazing / silhouette) and distance (near under 8 m / mid / far over 30 m), edge vs flat: the picture before the TAA
  against what it shows (luma change in vs out, flicker share over 1/16); and for the bright frame edges (edge pixels
  whose 3x3 luma span is over 0.25): history age mean / max, the history's weight in what is shown, rejection
  clip-only / depth-only / both / neither, the 3x3 YCoCg spread and variance-box size, the 3x3 depth spread, bright vs
  dark share, and 8 of those pixels' own readings (position, age, weight, rejection, luma, spread, depth). A TAA SEAM
  VERDICT line names which of three readings matches (CANDIDATE): F1 age resets and the clip rejects (the clip too
  hard on high-spread pixels), F2 age resets and the depth rejects (the depth tolerance), F3 the age climbs and it
  still flashes (coverage: `vr_ss 110`). Nothing in the picture changes: no TAA, weight, clip, jitter, Halton or depth
  reprojection change; with it on and `vr_taa_depth` off it allocates the trace's age buffers (about 124 MB of VRAM at
  2688x2880). `vr_mip` was not done (it only changes texture sampling inside surfaces, not silhouettes).
- **Fix: send-me.zip has every recent session's logs.** collect-reports.ps1 took only the newest report's logs, so a
  test of several sessions in a row sent the last session's alone (visual-9: two earlier sessions with only their
  window-probe files). It now adds the console / game / prompts logs of every session of the last 12 hours (up to 8),
  including one still running (its console log is written as it goes); the report json stays the newest only.
- **A-depth TRACE (diagnostics only, nothing new on by default; `vr_taa_depth` stays off by default).** Every 10 s,
  for whatever `vr_aa` / `vr_taa_depth` / `vr_ss` is in force (each window is tagged with them and the eye render /
  headset size): fps and the pair-to-pair frame-time spread, render hitches (pairs 50 ms or more apart) on their own
  lines with a QPC time; with `vr_taa_depth` on: each eye's depth (this pair's and the shown one's), the depth path
  vs head-motion fallback share, the per-eye choice a per-eye gate would make (logged only: rendering stays
  both-eyes-or-neither), the convention round trip mean / max, the history age of 8 static-surface pixels per eye,
  and the counted pixels binned by the surface's angle to the view (frontal / mid / grazing / past 16x anisotropy /
  depth edge / no depth) with clip-only / depth-only / both rejection, per-pixel fallback, history age, luma change
  and depth residual, and inside each bin the EDGE pixels apart (a depth step, a no-depth neighbour or a crease: the
  corner where a wall turns into a recessed window), bright SPIKES (a pixel above its own 3x3 variance box) and
  FLIPS (luma change over 0.25 against the history), the edge luma change in vs shown (pre / post TAA) and the edge
  history age mean / max. The TAA, its weights, clip, jitter and depth reprojection are
  unchanged: the trace is a separate pipeline variant used only with `vr_taa_depth` on, which only adds counters;
  if it cannot be made the counted depth path runs exactly as before.
- **SFX DIAG (diagnostics only).** The mod's own XAudio2 sound-effects layer: a format line (mastering voice, device
  mix format and name, our voices), 10 s lines (buffers, queue, glitches, errors, audio pass gaps, late wake-ups)
  and moment lines (glitch, critical error, late pass, layer switch) on the same QPC clock as the render hitches; no
  per-callback disk logging. New live.txt `vr_sfx_layer` (0 on, the default; 1 off: only the mod's own sound effects
  stop, live, no restart).
- **vr_ss now reaches the eyes with `vr_aa = ours`.** SpidyXrStart had put every `vr_ss` back to 100% under ours and
  SMAA; with ours it now keeps 110 / 125 / 150 (per side; 110 is new, 1.21x the pixels, a test). Default (off) is
  100% as before; with SMAA it stays 100% (SMAA is skipped on supersampled eyes).

- **Fix (A-depth CHECKED v3): the TAA FRAME LOG is read and printed once per 10 s window again.** 5881bac had moved it
  under the depth check, so the launcher read its 70 KB record from the game and printed a console line with every sample
  (2701 lines in the 16:35 session instead of about 17). Launcher only; the DLL, SPIDY TAA and `vr_taa_depth` (off by
  default) are unchanged.
- **A-depth CHECKED (TEST 0.0.76 A-depth CHECKED): the depth is checked before it is trusted, and its effect is
  measured.** Still off by default (`vr_taa_depth`). (1) Convention check: about once a second (one pair in 30 until it
  passes), both eyes' centre band of depth is read back. The centre pixel is put back in the world through the inverse
  of the JITTERED eye view-projection - rebuilt from the same CONFIRMED lens inputs (0x2c75560: the eye's tangents, the
  near and far the lens was given, this pair's jitter) - and projected back through the same matrix; the error in
  pixels is logged and over 0.05 px refuses the depth. That round trip only proves the matrix and its inverse, so a
  stereo test at depth edges (each left-eye point, read in each convention, must land on the same depth in the right
  eye) tells reversed-Z / 0..1 vs -1..1 / linear vs reciprocal apart. The log line states the convention
  (reversed-Z, D3D 0..1, reciprocal, finite far); any other fitting better is FAILED with "likely convention error"
  and the depth path stays off for the session. Until a check has PASSED the history is turned by the head alone.
  (2) Rejections reported apart, never combined: clip alone, depth test alone, both, per still / moving / fast swing
  (head travelling 6 m/s or more, or turning 120 degrees a second or more), for the depth path and the head path.
  (3) Per-eye gating: if either eye's depth (this pair's or last's) is missing, both eyes are turned by the head for
  that pair; the kept pictures are the same either way, so neither history is spoilt. (4) A pixel whose depth is
  0, 1, NaN or lands outside the last eye's frustum falls back to the head-motion place (history kept, counted as
  fallback), not rejected. (5) No motion vectors, no object motion: static geometry only. (6) Every 10 s the console's
  `TAA DEPTH` line: depth found per eye %, round trip px, clip / depth / both %, fallback %, fps and the depth pass's
  own GPU ms a pair (its two depth copies plus what the TAA pass costs more with the depth). In the report's
  `taa_depth_check`. TAA core (per-eye history, Halton 16, Catmull-Rom, YCoCg clip, 0.9 / 0.6, cut reset), the lens
  call, sharpening and the game's TAA unchanged.
- **A-depth (TEST 0.0.76 A-depth): SPIDY TAA's history follows the eyes' depth.** Until now each eye's history was
  turned with the head's turn only, so windows and fine building edges that shift against the far city as your head
  moves were clipped away and started again (the aliasing and the swing smear). Now each pixel is put back in the
  world with the eye's own depth (fmt 19 at the eye's size), moved into last pair's eye and looked up there; if last
  pair's depth there disagrees, the pixel was hidden before and starts fresh. The blend (0.9 still / 0.6 moving) and
  the colour clip are 509bdc9's, unchanged. live.txt `vr_taa_depth` is `off` by default (509bdc9's history) because
  the eye depth is still CANDIDATE (test 07c1675's log: both eyes' depth in 82 of the first 200 pairs); `vr_taa_depth
  = on` tries it, `auto` turns it on by itself once the TAA WATCH depth log (200 pairs) has CONFIRMED the eye depth
  (live, no restart). A pair without both eyes' depth is turned as before, by
  itself. Moving cars and people are still kept clean by the clip only (no motion vectors). About 190 MB of VRAM
  while on.
- **TAA FRAME LOG (read-only, jitter diagnostic build):** SPIDY TAA's history pass, eye 0, the last 300 pairs: the
  view each pair was rendered with and the one its history was fetched from (matrix and lens), our sub-pixel step on
  each, the centre pixel's history UV and shift, that shift with the step's change taken out (the drift), its change
  from the pair before, and a still-head flag. Every 10 s the console prints the mean / max drift with a still head
  and the measured step gain; at the start the line worked out from the code: `matrices: jittered, jitter applied:
  once, sign: +`. The report's `taa_frame_log` keeps the stillest 300-pair block and the last one. No picture changes.
- **TAA WATCH (read-only, in every 0.0.76 A/B/C test build):** the console prints a line every 10 seconds with the
  eye fps, the GPU time a pair (the game's two eye jobs, timed at their own begin and end markers, and SPIDY TAA's own
  pass) and the still / moving split, including how many pairs with a still head were treated as moving (over 20%
  means the thresholds are wrong, not the blend). It also logs the eye depth (fmt 19 at the eye's size) for the first
  200 pairs and says CONFIRMED or CANDIDATE. All of it is in the report's `taa_watch`. Nothing to set; no picture
  changes.
- **Tests for the far windows that swim with a still head (in every 0.0.76 A/B/C test build):** each line goes in
  live.txt, takes effect at once, and can be changed between runs; without them nothing changes. `vr_taa_jitter =
  on` (the default) / `off` / `fixed` / `half` changes only our own sub-pixel step (off: none; fixed: one constant
  offset; half: the same 16 steps at half size) - our TAA and each eye's history keep running, the game's jitter is
  untouched. `vr_taa_still = 0.97` changes only how much of the old picture is kept while the head is still (this
  build's own when unset: 0.9, C-still094 0.94; the moving share stays 0.6). TAA WATCH's 10-second lines now also
  give the history rejection - the share of pixels the TAA's colour clip had to correct - with the head still and
  moving, and which vr_taa_jitter / vr_taa_still the window ran with. And a JITTER CHECK with the head still: our
  step this pair and last, how far the TAA moved its history to line it up (in pixels), and whether that counts the
  step none, once or twice (once is right; the code does it once).
- **SPIDY TAA back to 509bdc9's picture:** test 9aaedd5's tune (a longer average, a looser colour clip, a 10-bit
  history, a mild sharpen after the TAA and texture mip bias -0.5) brought the line crawl back, so all of it is taken
  back: the eyes get exactly 509bdc9's TAA again, which fixed the shake. Nothing to change in live.txt. Kept, off by
  default: `vr_ss = 125` or `150` in live.txt renders the eyes larger and filters them down first - smoother, but
  heavier on the GPU (next VR start). The report now keeps the "VR AA" record even when the game closes first.
- **Our own eye anti-aliasing (SPIDY TAA), the default:** the game's TAA no longer runs in the eyes - its one history
  was made for the flat camera, which brought the far shake and the ghosting. Each eye now has its own history in
  the mod's own pass, moved with that eye's own pose (its own view, lens and position, this pair and the one before),
  its own sub-pixel jitter, and colour clipping so moving things leave no trail. Eyes at native 100%, no upscaler,
  supersample off, so it costs little. live.txt (applies the next time VR starts): `vr_aa = smaa` gives SMAA in each
  eye instead - no history at all, so nothing can shake or ghost (a little more shimmer); `vr_aa = game` is the old
  SM1STYLE picture. If our TAA cannot start, SMAA takes over by itself. The console's "VR AA: ..." says what ran.
  Not done: the game's depth and motion vectors for the eyes are not known (no CONFIRMED addresses), so the history
  follows your head only; moving cars and people are kept clean by the clipping instead.
- **Freezes and lag fixed (HITCH HOLD):** in the last test Virtual Desktop held about one frame in ten for a tenth of a
  second, and each held frame made VR drop to the game screen for up to two seconds (the freezes, black edges and
  snaps). A held frame of up to half a second now plays as one short step and VR goes on. live.txt, live (no
  restart): `hitch_hold = off` is the old rule. If it still stutters, check Virtual Desktop's Wi-Fi, bitrate and
  codec (90 Hz). [docs/VISUAL-0.0.76.md](docs/VISUAL-0.0.76.md)
- **Double jitter fixed (JITTER FIX):** the game's own (unseen) head view and the head view the eyes' shadows are
  fitted to took the game camera's already-stored TAA jitter and gave it to the lens again, which stores twice what it
  is given and flips the up-down part, so those views jittered twice as far, upside down. They now get none. The eyes'
  own jitter is unchanged and the game's TAA stays on in each eye. live.txt, live (no restart): `vr_game_jitter =
  keep` is c71a59c's doubled jitter, to compare. The console's "JITTER LOG" lists each view's jitter for its first 200
  frames, and "POSE CHECK" now also says whether each eye's layer got the exact pose and field of view its picture was
  built from, and whether those eyes were located at the frame's predicted display time (read-only). Note: the last
  time the head view had no jitter (c8974f4), the city ghosted; if it does again, say so - `vr_game_jitter = keep`
  puts it back live.
- **City ghosting fixed (GHOST FIX):** the ghosting of the city and buildings that came with the last test is gone:
  the game's own (unseen) head view keeps its TAA jitter again, as in the sharp dd027d4 build. Taking it away (the
  last FAR SHAKE try) stopped that view's TAA and did not stop the far shake (run 005811). live.txt, live (no
  restart): `vr_game_jitter = off` is that try again, as a test; `vr_jitter = off` gives the eyes one fixed offset
  instead of their sample walk (a test). The report now says whether each eye read only its own TAA history
  ("SM1STYLE eyes: ..." at the end of the console). Far shake: still open - see
  [docs/VISUAL-0.0.76.md](docs/VISUAL-0.0.76.md); with Virtual Desktop, try Synchronous Spacewarp Disabled
  (Streaming tab) as a test.
- **Far shake: render vs. headset pose checked (POSE CHECK):** the eye-to-headset path was traced end to end: each eye
  is drawn from the same OpenXR eye pose (one xrLocateViews at the frame's predicted display time) that its layer is
  submitted with, and a picture shown twice keeps its own pose, as OpenXR wants. No mismatch was found in the code,
  so no pose was changed. Each run now measures it: "POSE CHECK render vs submit: MATCH / DIFFERS" at the end of the
  console, with each eye's difference in degrees and millimetres, how far the game world turned in your room before
  the picture was shown, and the picture's age (`pose_check` in the report). Read-only: no setting.
- **Clearer picture (VISUAL):** VR now starts the game with its Anti-Aliasing at TAA and no upscaler, so each eye
  gets the game's own TAA (SM1STYLE) at native 100% - no DLSS, FSR, FXAA or CAS on the eyes. Texture Quality goes to
  at least High and filtering to 16x while VR runs. Your own settings come back when the game closes. live.txt:
  `vr_aa = keep`, `vr_textures = keep`, `vr_render = upscaled` (each applies the next time VR starts).
  [docs/VISUAL-0.0.76.md](docs/VISUAL-0.0.76.md)
- **Suits scanned for you (AUTO-SCAN):** the launcher makes sm2scan.json by itself the first time (or after a game
  update), in the background - "Scanning your suits (first time only)" in its log. PLAY VR never waits for it.
- **WORN SUIT (CANDIDATE):** the suit probe names the Peter suit you wear ("WORN SUIT: ..." in the console) after
  each suit change, and sends it to your friend in the MP packet (he logs it; nothing draws it yet).
  See [docs/REMOTE-SUIT-0.0.76.md](docs/REMOTE-SUIT-0.0.76.md).
- **Web shooters never web-zip punch (TRIGGER GUARD):** a trigger press only shoots the web ball. The shooting arm's
  thrust no longer becomes a web strike (zip to the enemy and punch) - not while the trigger is touched, for 0.45 s
  after it, nor while the other hand shoots. Punches from a free hand, the punch dash and the close snap are as
  before. live.txt `trigger_guard = off` brings back c8974f4's rules (live, no restart).
  [docs/COMBAT-0.0.76.md](docs/COMBAT-0.0.76.md)
- **Punch dash is back (PUNCH DASH):** punch toward an enemy 2 - 15 m away and Spider-Man web-strikes (the game's Y)
  right to **that** enemy; with no enemy along the fist the old lunge still takes far swings. Enemies within 2 m get
  the punch (X) as before - never a dash at a close one.
- **Web balls are the game's own (BALL GAME):** HERO VR never draws or fires its own balls or cocoons for
  Spider-Man. Each press sends the game's web shooter (RB) on the press frame, from the wrist. Out of combat the game
  still refuses it (its own move selector) - not yet fixable; there nothing visible happens.
- **Steady picture:** every VR session turns the game's dynamic resolution off (it was already off for the tester).
  The blocky picture while swinging is most likely Virtual Desktop's stream; see "PICTURE AT SPEED" in
  [docs/COMBAT-0.0.76.md](docs/COMBAT-0.0.76.md).
- **Spider-Man punches land (MELEE SNAP):** each punch goes to the enemy you swing at (within 2 m, inside 70 degrees
  of the fist, or the nearest within 1.8 m in front) - the game's X combo plus the blow on him. With an enemy that
  close the web lunge (Y) never steals the punch. Arm swings while walking still do nothing.
  See [docs/COMBAT-0.0.76.md](docs/COMBAT-0.0.76.md).
- **Web balls every press (BALL SURE, 32bd648; BALL NOW, 8046cd4):** both replaced by BALL GAME above.
- **Flight dives and climbs where you look:** the left stick's forward follows your look up and down (no need to
  hover to go down); the right-stick climb stays.
- **Roll camera:** FLY ROLL COMFORT (the default) now keeps the view level and rolls only the body; FULL is the old
  rolled view. See [docs/FLIGHT-ROLL-0.0.76.md](docs/FLIGHT-ROLL-0.0.76.md).
- **Suits on the other player (probe 5):** still CANDIDATE, no remote-suit change yet. See
  [docs/REMOTE-SUIT-0.0.76.md](docs/REMOTE-SUIT-0.0.76.md).

## hero-0.0.76-flight - the flight rework (DRAFT branch, not released)

From hero-0.0.76-sm1 (faecbe9). **Built and unit-tested, not yet tried in the game.**

- **New flight (SM2), only for heroes who fly** (Iron Man, Superman, Thor unless his hammer flight is off, Green
  Lantern): both hands straight up, or A held, takes off. You fly on by yourself; your hands steer, smoothly.
  Left stick forward/back sets the speed (back: a hover); full forward is **SUPERSONIC**, which only makes you faster
  and never lowers or grounds you. **A** is the flight tool (held in flight: come down and land), **B** the air brake,
  **holding Y** the orientation brake (rights you), the **right stick** rolls your body round the flight axis (full
  360, holds where you leave it). The flips keep the old non-realistic sensitivity; Thor's hammer grip stays behind the
  back. While flying A is never the game's jump and B never its dodge. R3 still lands.
- **Lights through buildings (Batman report):** the game's lens flares now stay off in VR for every hero, whatever
  live.txt or the hologram's LENS FLARES row says (GAME only keeps the flat game's own setting). The report was from
  the 0.0.74 backup with the row flipped to GAME; see [docs/LIGHTS-0.0.76.md](docs/LIGHTS-0.0.76.md).
- **live.txt `fly_classic`** (off by default): on brings back the 0.0.75 flight (grip take-off, poses, trigger
  thrust); live, no restart.
- **Walls:** only Spider-Man climbs or sticks to walls (also after flight and grapples); another hero touching a wall
  is pushed off it. Wolverine's CLAW CLIMB is gone and SPIDER CLIMB ONLY off no longer lets other heroes climb.
- **SM1 add-on:** the same flight (arms up or A held, arms steer, left stick speed, SUPERSONIC, B brake, A held
  lands); iilyn's graphics untouched. Not in SM1 yet: the body roll, the Y brake and the walls rule (no CONFIRMED
  export for them). The A/B buttons need SpidyVR's bindings to name them (CANDIDATE); arms up always works.

## hero-0.0.76-sm1 - SM1 hologram (DRAFT test build, not released)

**Built and unit-tested, not yet tried in the game.**

- **SM1 wrist menu is now the SM2 hologram:** the same SM2 panel code (frame, fonts, colours, tabs, portrait tiles,
  CONTROLS / OUTFITS / POWERS, COLOR under the tiles, off by default) painted onto a quad. Point with the right hand
  and pull the trigger to pick.
- **Picking a hero now works:** PLAY AS closes the hologram (powers, flight and grapple were blocked while it was
  open, which is why the first test did nothing) and shows what the hero does. Every power shot is logged.
- Still not in: a live suit/model change (no CONFIRMED call), health rows in SM1.

## v0.0.75.2 - HERO VR 0.0.75 hotfix 2: Spider-Man 1 heroes (DRAFT, not released)

Shows as v0.0.75 (VERSION unchanged; at release: a PORT-VERSION bump and launcher build 18). From hero-0.0.76-sm1 on
v0.0.75.1 (efac6a8). **Built, not yet tried in the game.**

- **Spider-Man 1 (iilyn's Spider-Man VR mod):** Mulder's Heroes add-on now runs inside the game as an OpenXR layer,
  registered for your Windows user only while Spider-Man 1 plays. His graphics, performance and anti-aliasing are
  untouched.
- **Wrist hero menu:** opens like SM2 (right hand on the left wrist, pull the right trigger; the same click closes
  it), with the SM2 hologram look; right stick and right trigger pick. Hero select, outfits (names and artists), the holo card.
- **Flight:** Iron Man, Superman, Thor and Green Lantern hold a grip to take off and stay up; the left stick steers
  where you look.
- **Hero powers** hit the enemy his combat has ringed, with a beam.
- The Spider-Man 1 page lists which hero features Spider-Man 1 has and which not yet.
- **Hero suits in Spider-Man 1 (new, not yet seen in the game):** the Spider-Man 1 page's Install suits button fits
  the ten heroes to the game's own suit on your PC and adds them to the game's Suits menu (the way Overstrike suit
  mods work; Overstrike / ALERT by Tkachov). Pick them in the Suits menu; the wrist menu does not switch them yet.
  Remove suits puts the game's files back exactly; Game info only reads the game and saves a report. Close the game
  first; each takes effect the next time the game starts. iilyn's files are never touched.
  See [docs/SM1-SUITS-0.0.76.md](docs/SM1-SUITS-0.0.76.md).
- Everything in v0.0.75.1 (the freeze fix on load) is included.
- Credits: iilyn (Ilya Mezerowsky) for SpidyVR; the model artists; made by Agent Mulder.

## v0.0.75.1 - HERO VR 0.0.75 hotfix 1 (October 31, 2026)

**EXPERIMENTAL BUILD: 0.0.75 may crash or have issues. 0.0.63 is the most recent STABLE build** (Revert > USE 0.0.75,
USE 0.0.74 or USE 0.0.63 goes back, settings kept).

Version 0.0.75 hotfix 1 (build Update 39.8.1), launcher build 17 (0.0.75.2). It still shows as **Version 0.0.75**: the
VERSION file stays 0.0.75 (a four-part 0.0.75.1 would read as older than 0.0.75 in every existing launcher), and the
newer launcher build is what offers it to 0.0.75 players. HERO_TEST_BUILD OFF.

- **Fixed: the freeze right after loading a save in 0.0.75** (the see-through depth lock). The see-through depth
  barrier no longer holds its lock while waiting for the command list lock, so the game no longer hangs on load.
- Everything else is the same as 0.0.75.
- **Spider-Man 1:** the heroes and the wrist menu are not working in Spider-Man 1 yet; that comes in the next hotfix.
- Launcher: Revert offers 0.0.75 first, then 0.0.74, 0.0.73, 0.0.63, 0.0.62, 0.0.6 and 0.0.5.

## v0.0.75 - HERO VR 0.0.75 (October 31, 2026)

**EXPERIMENTAL BUILD: 0.0.75 may crash or have issues. 0.0.63 is the most recent STABLE build** (Revert > USE 0.0.74
or USE 0.0.63 goes back, settings kept).

Version 0.0.75 (build Update 39.8), launcher build 16 (0.0.75.1). From the hero-0.0.75 work (a823158), HERO_TEST_BUILD
OFF. Revert offers 0.0.74 first, then 0.0.73, 0.0.63, 0.0.62, 0.0.6 and 0.0.5.

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

Known issues:
- Aliasing and window edges are still being improved.
- Damage to the green symbiote enemies is not confirmed yet.
- The '89 Batmobile can look grey.
- The motorcycle and the Spider-Man 1 option are untested in a headset.
- No passenger seats.

Credits: the motorcycle is "Low-Poly Sci-Fi Motorcycle - Cyberpunk Theme" by Mert Tetik (CC BY 4.0); the Spider-Man 1
option runs iilyn's SpidyVR (Ilya Mezerowsky, GPL-3.0) unchanged. Made by Agent Mulder.

## v0.0.74 - HERO VR 0.0.74 (October 31, 2026)

**EXPERIMENTAL BUILD: 0.0.74 may crash or have issues. 0.0.63 is the most recent STABLE build** (Revert > USE 0.0.73
or USE 0.0.63 goes back, settings kept).

Version 0.0.74 (build Update 39.7), launcher build 15 (0.0.74.1). From test build 10 (1d50e07) with the test notice
and TEST tag removed (HERO_TEST_BUILD OFF; the wrist shows "HERO VR 0.0.74 - MADE BY AGENT MULDER", the console
"HERO VR 0.0.74 - Made by Agent Mulder"). Multiplayer (the lobby server list) is included.

- **SM1STYLE eye anti-aliasing (on by default):** each eye uses the game's own TAA the way SpidyVR does, so edges
  shimmer much less. To go back to the old look, put `sm1style = off` in live.txt (it applies the next time VR starts).
- **Window-edge lights fixed:** lit windows no longer go dark at the outer edges of each eye.
- **The picture is back to 0.0.72's baseline**, keeping 0.0.73's flicker fixes (no lens flares or eye bloom in VR,
  steadier street lights and shadows, fewer popping far windows). 0.0.73's extra picture switches are gone.
- **Flying:** hold a grip to start flying, steering follows your body and is smoother, flips are livelier, and a tap
  on **Y** while flying looks up ahead when you lie flat. Flying no longer stalls or sinks when a hand blinks out of
  tracking.
- **Daredevil quick grapple:** a faster, smooth pull, and pressing grapple again with either baton mid-pull
  re-aims it at once (Arkham style).
- **Roster:** a **COLOR** tab in the header and a **MUSIC** tab on each hero with the music player.
- **Music kept:** your songs live in Documents\HERO VR\Music, so updates and reverts never lose them.
- **Multiplayer:** an internet **server list** (HERO VR's own free Cloudflare lobby), plus hosting and joining on
  your own network. The list only shows game names, heroes and player counts.
- **Batman:** four drivable **Batmobiles** (1989, ODAMI, FOREVER, FOREVER 2), the **Batcave** with the Batcomputer
  and a **safehouse** for the van, crime alerts with a route on the dash, glide pitch from your controllers' tilt,
  and separate belt spots for the grapple gun, batarangs and smoke pellets.
- **Cars:** the van and Batmobiles are solid on foot, don't show through walls, keep your play area facing forward
  as you get in, and turn flips off while you drive.
- **Throws** fly along your arm's swing (batarangs, shield, hammer, batons). Punisher's hook fires instantly, and no
  grapple press is lost.
- **Launcher (0.0.74.1, build 15):** safety checks (mod check, two-GPU check, clearer errors), Reports include the
  newest crash dump, and **Revert** offers **USE 0.0.73** first, then 0.0.63, 0.0.62, 0.0.6 and 0.0.5.

### Known issues (being worked on for 0.0.75)
- Aliasing is better but still needs work.
- Window edges are much better but not fully fixed yet.

## v0.0.73 - HERO VR 0.0.73 (October 29, 2026)

**EXPERIMENTAL BUILD: 0.0.73 may crash or have issues. 0.0.63 is the most recent STABLE build** (Revert > USE 0.0.63
goes back to it, settings kept).

Version 0.0.73 (build Update 39.6), launcher build 14 (0.0.73.1). From test build 6 (7af1589) with the multiplayer
test (MP SOLO TEST) left out, GAME TAA OFF by default, and the test notice and TEST tag removed (HERO_TEST_BUILD OFF;
the wrist shows "HERO VR 0.0.73 - MADE BY AGENT MULDER", the console "HERO VR 0.0.73 - Made by Agent Mulder").
The release zip carries `Models/` (with WOLVERINE's claws) and `Sounds/`.

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

## v0.0.72 - HERO VR 0.0.72 (October 28, 2026)

**EXPERIMENTAL BUILD: 0.0.72 may crash or have issues. 0.0.63 is the most recent STABLE build** (Revert > USE 0.0.63
goes back to it, settings kept).

Version 0.0.72 (build Update 39.5), launcher build 13 (0.0.72.1). From test build 12 (68b111d), with the test notice
and the TEST tag removed (HERO_TEST_BUILD OFF; the wrist shows "HERO VR 0.0.72 - MADE BY AGENT MULDER"). The release
zip carries `Models/` (every model and suit of test build 12) and `Sounds/`. Every hero stays OFF until picked; the
default suit is THE GAME'S OWN.

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

## v0.0.71 - HERO VR 0.0.71 (October 27, 2026)

**EXPERIMENTAL BUILD: 0.0.71 may crash or have issues. 0.0.63 is the most recent STABLE build** (Revert > USE 0.0.63
goes back to it, settings kept).

Version 0.0.71 (build Update 39.4), launcher build 12 (0.0.71.1). 0.0.71: fixes VR not starting after Stop VR
("handle is invalid" at finding the headset); includes everything in 0.0.7. No new game address; the mod is 0.0.7's.

- Fixed: after Stop VR, PLAY VR stopped at "finding the headset" with `[WinError 6] The handle is invalid`. Stop VR
  left the launcher's standard handles closed, and the next start handed them on. The launcher now gives PowerShell
  its own input, saves and restores its handles around Stop VR and clears any dead ones; the Python side starts every
  program with its own input and repairs dead handles first.
- New Windows test (std-handles): Stop VR, then PLAY VR again, reaches "finding the headset" without the error.
- Revert still offers 0.0.63, 0.0.62, 0.0.6 and 0.0.5.
- Launcher build 12: before any update, the launcher shows that the new version is EXPERIMENTAL, may crash or have
  issues, and that 0.0.63 is the most recent STABLE build you can go back to with Revert; it updates only after Yes
  (No is the default). Older launchers (0.0.7, 0.0.63 and before) show the same warning in their own update window.

## v0.0.70 - HERO VR 0.0.7 (October 26, 2026)

Version 0.0.70 (build Update 39.4), launcher build 11 (0.0.70.1). "0.0.7" is published as 0.0.70 because versions
compare number by number (7 < 63); the trailing 0 keeps it above 0.0.63 for every launcher (docs/VERSIONING.md).
No new game address: every blow uses the CONFIRMED damage path, healing and SHIELD BLOCK the CONFIRMED HeroHealth record.

- BATMAN grapple gun and cape glide (cape_glide, bridge `SpidyCapeSample`): aim a hand at least ~10 degrees up (no enemy
  in the batarang sights) and pull the trigger to fire the grapple at a rooftop or ledge 2.5-45 m away (line drawn,
  zip at up to 24 m/s, vault over the edge); the grapple gun model ("Batman's Grapple Gun" by Falxx, CC BY 4.0,
  `Models\GrappleGun`) is drawn in the firing hand with the line from its muzzle, holstered on that hand's hip otherwise; arms out in the air glide (lean/dip one arm to turn, tuck to dive, raise
  to pull up; drop the arms or land to stop). No webs and no flight for BATMAN.
- BATMAN batarang throw: the batarang shows in a hand only while its trigger is held (never idle, never during a
  grapple press) and is thrown on release at the target the aim assist picks then, on a slight curve (~12 degrees,
  mirrored per hand) that still ends on the target; CHAIN and the return are unchanged. No target: a slight arc onto
  the aim, then straight.
- BATMAN smoke pellet: a grip press starting at the front of the belt (0.55-0.95 m below the head, within 0.09 m of
  the middle, 0.05-0.40 m ahead; clear of the hip holsters) takes a pellet; letting go throws it with the hand's
  velocity; it bursts into smoke on the floor, on an enemy or after 3 s. Smoke no longer puffs on batarang hits.
- Webs are Spider-Man's only: with any other hero the bridge gets no web triggers or grips, no aim markers, no web yank
  and no Y web strike (a far punch lunges without a line). Live, no restart.
- New heroes, all OFF by default (one hero at a time): BATMAN (batarangs with BATARANG CHAIN, grapple gun, cape glide,
  smoke pellets - the smoke is a look only), SUPERMAN (heat vision, COLD BREATH - real tick blows, nobody is frozen -, super strength, the SUPERSONIC
  dash), PUNISHER (two pistols and a sawed-off, holsters, EASY RELOAD), WOLVERINE (claws hitting 3x a punch, healing) and CAPTAIN AMERICA
  (shield worn, drawn, thrown with SHIELD CHAIN, bash; SHIELD BLOCK is an OFF-by-default experiment).
- Guns fire along the barrel: pistols and sawed-off sit in the palm from the OpenXR grip pose and shots leave the drawn
  muzzle. Daredevil's batons sit in the fist. Valve Index 15-degree tilt: unconfirmed (CANDIDATE).
- Optional models (BATON LOOK, BATARANG STYLE, GUN MODELS, SHIELD LOOK) from `Models\` in the release zip, with a
  built-in fallback. Credits: THIRD-PARTY-MODELS.md and CREDITS.md. The JOKER batarang (CC BY-NC-ND 4.0) ships unmodified
  with its licence.
- MIRROR VIEW: kept across the restart the wrist asks for, and the test turns itself OFF (saved) after its 30 s run.
- Captain America suit: a manual guide (CapSuit/, docs/CAP-SUIT-PIPELINE.md); nothing installs it.
- Launcher build 11: smoother opt-in HERO VR design (60 fps menu, seamless orb loop at the concept speed); classic stays
  the default with its fallback. Revert offers 0.0.63, 0.0.62, 0.0.6 and 0.0.5.
- Smoother cutscene starts: the mod no longer asks the game for two hidden 3D eye views (about 1-1.25 s at every
  cutscene start or resume) while the HUD panel's hook, without which they cannot be shown, is not installed (SM2:
  never yet). Cutscenes stay on the flat screen as before; GAME SCREEN 3D's text says so.
- Fixed: the launch settings' `session_off` now reaches prepare_launch on the real launch path.
- Release cleanup: test logs untracked, debug notes go to the log only, build paths removed from shipped files.

## v0.0.63 (October 26, 2026)

Version 0.0.63 (build Update 39.3), launcher build 10 (0.0.63.1). The mod is 0.0.62's plus one fix.

- Fixed: 0.0.62 failing to start VR. PLAY VR stopped at "starting the game" with `NameError: name 'session_off' is
  not defined` (run_game_vr.py read a variable of main() inside session()). The lens-flare choice now travels with
  the launch settings. New gate: tests/launch_session_tests.py runs the real launch path (main -> session ->
  prepare_display) with stand-in Windows modules, plus a pyflakes check for undefined names in tools/*.py.
- New opt-in launcher design (HERO VR orb, soundscape, big menu). Classic stays the default; the launcher asks once.
  Options > Launcher switches either way; Shift or --classic opens classic once; if the new design cannot start, the
  classic one opens and says why, once.
- Revert offers 0.0.62, 0.0.6 and 0.0.5. Updating from any earlier launcher installs 0.0.63 exactly once.

## v0.0.62 (October 26, 2026)

Version 0.0.62 (build Update 39.2), launcher build 9 (0.0.62.1). Built on 0.0.6 and its hotfix. No new game address:
every hit uses the CONFIRMED damage path, every wall test the CONFIRMED ray bridge. Graphics defaults unchanged.

- Ghost hands: EYE AA no longer keeps history far outside a pixel's neighbourhood, so fast hands leave no copies.
- BOLT WALLS: repulsor bolts, the chest beam and web balls stop at the first wall (fail safe: no answer, no wall).
- LENS FLARES (GRAPHICS, takes effect at the next VR start): OFF IN VR turns off the game's flares (headlights and
  bridge lights through walls at night); GAME, the default, leaves them.
- Combat: PUNCH HITS (GAME + INSTANT by default) lands a small hit when the fist touches; ENEMY TRACKING (LIVE by
  default) follows enemies at 30 Hz by their own positions. 0.0.6 behaviour one setting away.
- Daredevil: batons hit in the hand and on straight throws; a thrown baton bounces between up to 3 enemies (next within
  5 m, never the same twice, skipping downed enemies) and always returns. BEACONS IN VISION (ON): beacons and markers
  stay bright and coloured in the sonar.
- Iron Man: a new CHEST BEAM sound made from the player's repulsor sound, and the bolts and the chest beam shimmer like plasma.
- Iron Man: the hand blast uses a player's own repulsor sound (thank you!), fires from the palm along its normal, and
  hand repulsor blasts have a 2-second cooldown per hand (a ring on each wrist). The CHEST BEAM does 4x a blast's damage
  with its 30-second cooldown shown on the wrist ring, as before.
- REAL ANCHORS (launcher, ON): a swing starts only when the web meets a surface within 100 m; otherwise a miss and a
  buzz. OFF keeps the air swing. HIT INDICATOR (OFF) and HAND FLIPS (ON = 0.0.6).
- Launcher build 9: Revert offers 0.0.6 and 0.0.5; numeric version compare; first-run fixes for bracket folders,
  running inside the zip and antivirus removing the launcher; crash log falls back to %TEMP%.

## v0.0.6 hotfix (October 26, 2026)

Version 0.0.6 (build Update 39.1.1), launcher build 8 (0.0.6.6; build 7 = 0.0.6.5 was the first hotfix launcher).

- Launcher build 8: fixes "No working Python 3.11 or newer was found" after updating to the hotfix. The update from
  launcher build 6 kept the old mod files for the same version 0.0.6 and, doing so, left out the mod's own Python
  (python folder). The launcher now checks python\python.exe and the mod files' build after every install, update and
  start, and before PLAY VR, and downloads the ready-made mod files again by itself when anything is missing or older.
  Players never install Python. The release mod zips (0.0.5 and 0.0.6) always had their Python; a new gate checks it.
- The update from launcher build 6 also now replaces the Update 39.1 mod files with the 39.1.1 ones (the swapchain fix).
- A blank or placeholder update-repository setting falls back to lecutis7of9/HEROVR.

- VR no longer falls back to flat screen when the headset runtime refuses the eye images at start ("Create eye
  swapchain XrResult=-2", seen with Virtual Desktop). The mod tries again with the 0.0.5 settings (the same request,
  after 0.5 s and 1.5 s), then without the transfer-source usage, then another colour format, then smaller eye sizes
  (90/80/70/60%), and writes every try to the console. If all fail it keeps retrying the session every 2 s (5 s after
  2 minutes, with advice to restart Virtual Desktop and the game). No game addresses changed.

## v0.0.6 (October 25, 2026)

Version 0.0.6 (build Update 39.1), launcher build 6. Built on 0.0.5 (with its hotfixes 2 and 3).

- Hero powers with real damage: IRON MAN (repulsors, the chest beam aimed at the enemy you look at), DAREDEVIL
  (batons that chain to up to 3 more enemies, DAREDEVIL PUNCH, sonar) and HULK (charged jump, knockouts). One hero at a
  time, all OFF by default, all live.
- New Iron Man repulsor look and sound (no new render pass).
- The hologram in 8 pages with named sections; RESTART tags on the settings that need one.
- Opt-in reports (crash report, Report a bug, play stats off by default); nothing is sent by itself.
- Frame rate (Update 39.1): measured 34.5-37 new frames a second at 90 Hz, GPU-bound (the headset copy waits about
  6.6 ms for 0.19 ms of work; Spidy's own work is under about 1 ms). Session advice for 72/80 Hz and RenderScale
  90/80 (+14% / +30%, estimates from the RTX 4080 measurement); AUTO PERFORMANCE's AUTO target is half rate when full
  rate is out of reach; stalls are counted only during play. Graphics defaults unchanged. No new game address.
- VR-loss fix (Update 39.0.3): a lost OpenXR session is recovered (the mod retries every 2 s) instead of leaving the
  game flat.
- Launcher build 6: Revert > USE PREVIOUS VERSION (0.0.5) downloads and SHA-256 checks the v0.0.5 release, installs
  it and skips 0.0.6 until Updates > Update now.
- `tools\build.ps1` run by a launcher installs the release's ready-made mod files (never compiles); `-FromSource` builds.

## v0.0.5 (October 25, 2026)

- Launcher build 4 (hotfix, mod files unchanged): VR lost (e.g. back to the desktop in Virtual Desktop) or a headset
  that is not connected no longer leaves the game silently on its flat screen. PLAY VR says "Headset not connected:
  open Virtual Desktop in your headset first" and waits for it (up to 5 minutes) without starting the game; a
  Spider-Man 2 left running is offered to be closed first; when VR is lost mid-game the launcher offers to close the
  game and start VR again (VR cannot restart inside the same game process in 0.0.5).
- First public HERO VR release, with the HERO VR launcher (new background, "Made by Agent Mulder").

**Version 0.0.5 (build Update 38.9, October 25).** Built on 0.0.4. The player, after the 0.0.4 headset test: punches
still miss ("it needs to register every single hit"), the damage path should be "the same logic as a punch", a hero
height for a big skin, and a Daredevil baton that chains. Then: the beam's cooldown on the wrist, more forgiving
punches, a faster FLY with a SUPERSONIC dash, and sound effects. Every new experimental setting starts OFF; none
needs a game restart. The zips are sm2-vr-0.0.5.zip and sm2-vr-0.0.5-slim.zip (under 25 MB, no docs or images).

- PUNCH SENSITIVITY: FORGIVING (default) / MAX / 0.0.4 / STRICT (new, live). FORGIVING takes slower (0.78x of 0.0.4's
  punch speed), sloppier (0.7x the forward share, 0.75x the reach) and quicker re-punched jabs (re-armed after a
  0.4 m/s pull-back). Replay P5: rep0030 177 blows (0.0.4 151) of 169 forward fists over 1.2 m/s, rep0023 175
  (0.0.4 155) of 174, each its own X, mean delay 7-9 ms; 9/9 synthetic punches land (0.0.4 8/9: a 5-jab flurry with
  8 cm pull-backs was 1 blow, now 5); 14/14 everyday motions (waving, a hand dropped, scratching the head ...) send
  no X; run38/run381 waving with no enemy: X 0. MAX takes any forward hand (shown only, not sent, with no enemy).
- DAMAGE PATH (the same logic as a punch). Read from Spider-Man2.exe: the game's melee site 0x8558c7 builds the
  victim's handle from the actor table slot and queues a request with the DamageSystem through DirectDamage
  (RVA 0x3412ec0), which takes a pool slot from 0x3412da0 (lock xadd). STATIC-STRONG, CANDIDATE live.
  - DAMAGE PROBE (read-only, always on): the nearest enemies' hit points (BotHealth, found the way the CONFIRMED
    HeroHealth is) at 10 Hz, every fall put beside the last X press. The log says "VERSION 0.0.5 DAMAGE PROBE":
    CONFIRMED once an enemy's HP falls within 1 s of an X.
  - DAMAGE TEST: OFF (default) / ON (EXPERIMENTAL, live). ON sends one DirectDamage request per web ball or repulsor
    hit, the melee site's own way, after checking DirectDamage's bytes, the system's vtable and that the victim is a
    live actor slot; every touch is SEH-guarded and a fault turns it off (no crash). CANDIDATE until the probe sees
    an HP fall after the mod's own request with no X press (write state CONFIRMED).
  - Until then Iron Man's damage, the DAREDEVIL / HULK switches and the baton chain in the game stay unbuilt (no
    do-nothing switches); they are built as soon as the headset log says CONFIRMED.
- HERO HEIGHT: 1.00X (default) .. 1.60X (new, live). The VR body (not the view) is k times the player, so a big hero
  skin (Hulk) stands straight instead of crouching. 1.00X changes nothing.
- DAREDEVIL BATON CHAIN (logic, tests and preview; in the game once damage is CONFIRMED): a hit chains to the nearest
  other enemy within 6 m of the one hit, never one already hit on that throw, at most 3 chains (4 enemies), then
  returns; a miss or no enemy in range returns at once.
- BEAM RING: IRON MAN's chest beam cooldown is now a ring on the right wrist that fills clockwise over the 30 s,
  then flashes with a chime and a buzz when ready; the floating "BEAM n S" line is gone. Preview
  images/beam-ring-005.png.
- FLY MODE: FAST is now 40 m/s (was 28). SUPERSONIC: ON (default) / OFF and SUPERSONIC SPEED 2X (default) / 1.5X /
  2.5X / 3X of FAST (80 / 60 / 100 / 120 m/s), both new and live. Flying with both hands out (the FORWARD pose), push
  the left stick forward past 60% to dash; let go (under 40%) or drop the pose and you are back to FLY exactly as
  before. When the dash first kicks in (and again only after 1 s off): a sonic boom sound and a 0.9 s shockwave, a
  ring that rushes past you and a vapour cone flaring back. Nothing else in FLY changes: SLOW, NORMAL and the poses,
  turn, roll, rise, landing, the vignette and INVINCIBLE are as in 0.0.4. Preview images/supersonic-005.png.
  Honest: the game moves the hero at most 2 m a game step, so the top speeds need a high frame rate (at 60 fps the
  dash tops out near 105 m/s, so 3X reaches about 2.5X; at 72 fps and up all four differ). The flight speed is in the
  log ("flying n m/s now") and is CANDIDATE until a headset run; very fast flight may outrun the game's world
  streaming (low-detail buildings for a moment).
- SOUND EFFECTS: the mod's own original sounds (procedural, tools/sfx/make_sfx.py; assets/sfx): repulsor blast,
  chest beam, beam ready and sonic boom (a baton whoosh and hit are built in for the batons). They play on their own XAudio2 thread (loaded at run time;
  if audio fails: silence and a code in the log, the game and VR untouched). SFX VOLUME 100% (default) / 75% / 50% /
  25% / OFF (new, live). The game's own audio engine is not used (a hook stays CANDIDATE).
- PACKAGING: build.ps1 runs bootstrap.ps1 by itself when third_party is missing and names what is missing; the
  release gate unzips the slim zip into an empty folder, adds only what bootstrap fetches, checks every Python import
  and every file CMake names (tools/package_check.py), then builds and runs the tests there.
- Kept: no flicker, the left-wrist health ring, no kicks from waving, zip kick, grab assist, yank, WEB ROPE, graphics
  defaults, FLY MODE, INVINCIBLE, NOMINMAX project-wide and the windows header gate. Save and story flags untouched.
  Tests: 589 unit tests, tests/v005_tests.py, replay P5. "VERSION 0.0.5" on STATUS, in the log and the reports.
