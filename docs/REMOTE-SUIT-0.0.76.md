# 0.0.76 REMOTE SUIT: PLAYER 2 in his real, worn suit from your own game files

Goal (pt1572u): the friend is the normal in-game Spider-Man in whatever suit he wears, read from the
game files each PC already has installed. Nothing is drawn by hand, downloaded or redistributed.

## CONFIRMED
- The suit list is `configs/system/system_progression.config` (asset id 0x9C9C72A303FCFA30):
  SuitList.Suits[].Item -> reward config -> DefaultLoadoutConfig -> loadout Items[] -> vanity configs ->
  ModelList[].Model (`....model`). Format knowledge credited to Overstrike (GPLv3).
- toc / DSAR archives / Config_I30 layout and the game's asset id (CRC-64 >> 2, top bit) - read by
  `tools/sm2_suits/sm2scan.py` (read-only) and checked by tests/hero076_sm2_scan_tests.py.
- The instance word +0x88/+0x8c is a render flag set, not a suit id.

## CANDIDATE (needs one test run)
- Which compression the model blocks use (GDeflate needs a decoder port; LZ4/raw are read today).
- The model file's DAT1 type and sections (plans the mesh/skeleton/texture reader).
- Where the sender's worn model path lives near the player: `live.txt suit_probe = on`
  (read-only, logs each change of the worn .model set to the console).

## Test steps
1. `python tools/sm2_suits/sm2scan.py "<Marvel's Spider-Man 2 folder>" sm2scan.json`
   (the launcher's bundled Python works). It only reads the game files.
2. Put `suit_probe = on` in live.txt, start VR, load into the city, change suit three times in the
   suit menu (note which ones), then send the launcher console log and sm2scan.json.

## Still to build after that
GDeflate (if used), the model mesh + skeleton reader, retarget to the PLAYER 2 rig, BCn textures to the
GPU, the suit's asset id in the MP WORLD packet, and drawing it on the receiver - several days of work.

## Run 1 (2026-11-01 12:13 ET, ce7ea4e) - probe never ran
- The SM2 console log has no `SUIT PROBE` line at all (not even "on"), so `suit_probe` was not on in the
  launcher's own live.txt (`<launcher folder>\live.txt`); `live_changes` is empty. No .model paths, ids or
  offsets were captured; the "where the worn path lives" item stays CANDIDATE.
- The game's suit preview (864x480 target) opened 3 times (37 s, 45 s, 77 s); suit_watch changes 0.
- Clean exit: "Quit request confirmed by user" at 12:15:46, error none, no flat-screen.
- sm2scan.json was not sent (Reports does not collect it).
- Next: set `suit_probe = on` in that exact live.txt before launch, run sm2scan.py with the package's
  python\python.exe, repeat the three suit changes, send send-me.zip + sm2scan.json.

## Run 2 (2026-11-01 12:33-12:36 ET, ce7ea4e, suit_probe on) - probe ran, worn suit not found
- Probe on ("live.txt (kept): SUIT PROBE on"); 4 "change" sets logged (one at load, one after a reload,
  two when a phone prop came and went). The game's suit preview opened 4 times (30 s, 46 s, 83 s, 123 s).
- Every set: `characters/hero/hero_spiderman_base/hero_spiderman_base.model` (0xb742d90d7b153bdf) at
  record+0x030 -> +0x038 and `characters/hero/symbiote_wave_dash/symbiote_wave_dash.model`
  (0x9aadfe4146697e64) at record+0x030 -> +0x0c8. Same paths every time: CONFIRMED present, CONFIRMED NOT
  to track suit swaps (the base body, not the suit).
- Noise: `objects/fx/fx_poi_quest_marker_pillar.model` (record+0x270 -> +0x0c8) and
  `characters/prop/att_phone/att_civ_phone.model` (record+0x0f0 -> +0x038).
- No suit-specific .model reached in 2 hops (0x400 / 0x100 bytes). The worn suit's location stays CANDIDATE:
  deeper (child actors / vanity components), or a loadout/vanity .config rather than a .model.
- Clean exit ("Quit request confirmed by user" 12:36:35), error none, no flat screen; 3.3 s of stream stalls
  (Virtual Desktop).
- Next: sm2scan.json from the user's PC (the box has no game files) gives each suit's .model/.config ids;
  then a probe v2 searches near the player for those exact ids/paths (deeper walk, .config too).

## sm2scan.json from the user's PC (2026-11-01, pt1592u)
- 'sm2scan 1': 238 archives, 1,423,796 assets, 96 suits (51 for Peter, 45 for the second Spider-Man),
  283 different ids (each suit's loadout .config, its 1-2 .models, its mask). Every suit model is present and
  is LZ4 (228) or raw (2): no suit model needs GDeflate. The last 4 suits are the user's own mods
  (one path has spaces, so PROBE 2 matches it by id).
- `"missing": 22` counts ARCHIVES the toc lists whose file is not on disk (sm2scan.py could not open
  them: language/optional packs not installed, or empty mod slots), not blocks; none of the 96 suits' files
  are in them. `"raw": 32` are archives with no DSAR block header (read as they are).
- GDeflate: 119,430 blocks in other archives use it (CANDIDATE: textures, the next step); a GDeflate decoder
  is only needed when the suit textures are read, not for the models or this probe.

## SUIT PROBE 2 (same `suit_probe = on`, still read-only)
- Reads `sm2scan.json` from the HERO VR folder (beside the launcher), its `reports` folder, the Desktop
  (also OneDrive's) or Downloads, and says so in the console ("SUIT PROBE 2: 96 suits, 283 ids from ...").
  Without it, one line says where to put it and only PROBE 1 runs.
- Searches breadth first from the record, its instance and the hero component: up to 5 pointer steps,
  0x200 bytes of each object, at most 6000 objects a pass, 16 objects a frame, a 2 s pause between passes;
  never into the game's exe. Every read is first checked with VirtualQuery (committed, readable, not a guard
  page) and then made through the one SEH-guarded peek; a frame stops after 48 failed reads.
- Matches any 8-byte word that IS one of those ids, and any .model/.config path whose id is one. Each time the
  set of found ids changes it logs "SUIT PROBE 2: change N ..." with the suit (or "shared by N suits"), kind
  (loadout/model/mask), path, id and chain, e.g. `record+0x030 -> +0x038 -> +0x010 +0x008`, and a
  "worn suit (CANDIDATE ...)" line when exactly one suit is named by an id only it has.
- Every place it prints is CANDIDATE until a run shows it following the swaps.

## Run 3 (draft 2a86d3b) - PROBE 2 never ran
- No SUIT PROBE line: `suit_probe` was off in the new folder's live.txt. No crash. The SM1 branch since
  (e7362f6) starts the probe on in TEST builds and logs one start-up line every session saying so and where
  sm2scan.json was found.

## Run 4 (draft e7362f6, 2026-11-01 15:31 ET, pt1611u: Advanced Suit 2.0, then Classic, then Hybrid)
- Loaded: "SUIT PROBE 2: 96 suits, 283 ids from ...\Downloads\sm2scan.json". 14 changes, clean exit.
- NOT CONFIRMED: none of the 9 ids of those three suits (their loadouts, models and masks) was seen once.
- Why: every pass hit the 6000-object cap, so each pass read different memory and the set flickered (43-51
  "suits named at once"); most hits were loadout ids in one list (the unlocked suits); the 12-line cap hid
  the rest. The only model ids seen were two other suits' bodies, deep under record+0x030 -> +0x080.

## SUIT PROBE 3 (same `suit_probe`, still read-only)
- The same memory every pass: breadth first from the record, instance and hero, words in offset order; what
  is at or under the depth the object cap (now 10000) cut is not counted, so every pass counts the same
  objects ("complete to depth D" in each line).
- A set is told only when two passes in a row agree; after the first set ("baseline") only the changes are
  printed ("+" added, "-" removed with where it was), up to 40 lines a change, and a "swap ... A -> B" line
  when one suit's own ids go and another's come.
- An object holding 3 or more loadout ids is a list (the unlocked suits): its ids are counted, never shown,
  and it is not walked into.
- Loaded models: an object holding a suit's model/mask id or a pointer to any .model path is read 0x400 bytes
  wide (not 0x200) and walked 2 more steps (past the 6-step depth); its children are read wide too.
- WATCH: for i30_Advanced_Suit, SUIT_CLASSIC and SUIT_NWH_HYBRID (the names his sm2scan.json gives the run-4
  swaps), every place their model and mask ids are seen (up to 16 each) is logged as "SUIT PROBE 3: WATCH
  ..." when those places change (two passes agreeing), or "none of its model or mask ids in the memory read".
- Throttle: 24 objects a frame (about 10 s a pass at 45 fps), a 2 s pause between passes, 48 failed reads stop
  a frame; VirtualQuery then the one SEH peek, as before. Every place is CANDIDATE.
- Test: load into the city, wait about 30 s (two passes for the baseline), swap to Advanced Suit 2.0, wait
  about 30 s, Classic, wait about 30 s, Hybrid, wait about 30 s, quit, Reports, send send-me.zip. A chain is
  CONFIRMED only when its WATCH or "+" lines follow those swaps in that order.

## Run 5 (SUIT PROBE 3: Advanced Suit 2.0, then Classic, then Hybrid)
- Classic's and Hybrid's model ids were tracked in one container: A = *(record+0x080), B = *(A+0x028); the
  entries sit in B's words (seen at B+0x008 and B+0x188, and they move), and each entry E holds the model id
  at *(*(E+0x0e8 | +0x0f0 | +0x128) + 0x28). Advanced 2.0's id never showed up. CANDIDATE: one run, two of
  the three swaps.
- The end of the log looked like a crash; the user says he closed the game badly. Not a crash, nothing changed.

## SUIT PROBE 4 (same `suit_probe`, still read-only, guarded)
- Reads that container directly about once a second: record+0x080 -> +0x028, 0x80 words of the list, each
  entry 0x40 words wide, and one step into each pointer an entry holds (0x10 words). 8 entries a frame, 64
  failed reads stop a step, never inside the game's exe, VirtualQuery then the one SEH peek, as before.
- A set is told only when two passes agree: a baseline, then only changes, one "=", "+" or "-" line per place
  ("... at record+0x080 -> +0x028 -> +0x188 -> +0x0e8 +0x028"), then "suits now: ...".
- PROBE 3 now diffs by place, not by id: the same id moving to a new place is "+1 -1", not silence.
- Every probe line carries "[t+12.3 s 16:57:16.123]" (seconds since the probe went on, and local time), so
  lines can be matched to the swaps.
- Cost: every 30 s and when the probe goes off, "SUIT PROBE cost: N frames, mean X us, peak Y us ..." - what
  the probe costs the game thread.
- Test: start in some other suit, load into the city, wait 30-40 s, swap to Advanced Suit 2.0, wait 30-40 s,
  Classic, wait 30-40 s, Hybrid, wait 30-40 s, quit normally, send the log. The container is CONFIRMED only if
  PROBE 4's lines follow all three swaps in that order.

## SUIT PROBE 5 and the probe hitch (same `suit_probe`, still read-only, guarded)
- PROBE 5: the Hybrid suit shares its model with its sibling and differs only in its materials / textures and its
  equipped loadout config. `tools/sm2_suits/sm2scan.py` (sm2scan 2) now writes, per suit, "loadout" (the reward's
  DefaultLoadoutConfig), "vanity" (its item configs) and "materials" (.material / .texture paths). PROBE 5 reads the
  hero record and its instance and one step into each pointer there, about once a second, a few objects a frame,
  and logs "SUIT PROBE 5: baseline / change N ..." with the worn look (the suit most of the found ids name) and up
  to 6 +/- lines, when two passes agree. Every place is CANDIDATE. An older sm2scan.json says once to re-run the
  new sm2scan.py.
- PROBE 5, the last suit run (every candidate at once):
  - Ids: the look ids (materials / textures, the equipped loadout config, vanity items) and each suit's own item
    (the reward config id an equipped-suit setting would hold), model and mask ids.
  - Where: the hero record, its instance and the hero entity, two steps into what they point at (the hero's
    components and their material overrides), each object once a pass (loops read once), at most 3000 objects a
    pass, 32 a frame and never past the frame's 160 reads / 1.2 ms.
  - Save / profile: no address for the save's or profile's equipped-suit setting is known and none is made up; its
    suit item id is looked for wherever these reads reach, and a hit is CANDIDATE like every other place.
  - Record fields: the record's and the instance's own 0x80 words each. A word that changes in play outside the
    menus is noise and left out; the rest is kept when the menu opens and compared once two passes after the exit
    agree: "SUIT PROBE 5: menu exit N: K steady field(s) ... changed (CANDIDATE)" and up to 6 "record+0x... old ->
    new" lines.
  - The menu: "SUIT PROBE 5: menu N open" and "menu exit N after S s" (no gameplay camera for 0.5 s; a flicker is not
    told). A change seen in the menu ends "- in menu N", within 20 s of an exit "- S s after menu exit N".
- Less noise: PROBE 3 shows at most 6 lines a change and 2 WATCH places a suit (then "... N more"); PROBE 4 at most
  6; PROBE 1 shows only `characters/` paths (no fx quest-marker model).
- The hitch (run TEST 3c47385: peaks of 38-78 ms): PROBE 1 read ~4,000 times in one frame each second; it now
  reads 4 root words a frame. Every probe step stops between objects after 160 reads (PROBE 3: 600) or once the frame's 1.2 ms
  (all probes) is spent, and always does one object, so none starves. The cost line shows whether the peaks went.

## Drawing a friend in his game suit (started; nothing draws yet)
- Container formats (toc, DAT1, DSAR, LZ4 blocks): CONFIRMED by the user's sm2scan run.
- Inside a .model (joints 15DF9D3B, meshes 78D9CBDE, built 283D0383, vertexes 2C2CA205 and u16 indexes
  F4EA9E18): CANDIDATE, taken from ALERT's dat1lib for Rift Apart. include/spidy/sm2_model.hpp reads them
  and checks each guess ("ok: ..." / "no: ..."); vertexes and indexes are handed out only when the size
  checks hold. tools/sm2_suits/sm2model.py runs the same checks on the real game files:
  `python tools/sm2_suits/sm2model.py "<game folder>" sm2model.json` (read-only; `--all` for every suit,
  `--suits A,B` for some). Send sm2model.json back: the layouts become CONFIRMED only if every check says ok.
- MP: the 12-byte 'S' GAME SUIT tail (mp_net.hpp): 'S', 1, flags (1 known), 0, the worn suit's loadout id
  u64. Sent after WORLD only when the packet still fits 512 bytes; older builds ignore it. Nothing fills it
  yet (the probe's place is CANDIDATE).
