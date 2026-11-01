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
