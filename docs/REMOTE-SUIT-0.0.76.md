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
