# HEROVR renderer/gameplay parity — acceptance contract

Owner requirement (2026-11-10): **Original and Revamped must share all gameplay mechanics**. The selected renderer may change visual presentation and performance only. This applies to web swinging, enemy zip/yank, stealth webline and takedowns, sprint, wall movement, prop throw, symbiote controls, mission interactions, and all future gameplay changes.

## Existing 79.1 limitation

The successful combined preview workflow `.github/workflows/dynamic-body-0791.yml` explicitly copies the **official unchanged 0.0.79 Original DLL** into `renderer-modes/original`, while copying the freshly compiled 79.1 DLL into `renderer-modes/revamped`. Thus that ZIP does **not** establish gameplay parity. Keep it available for existing Revamped headset testing; do not publish it as a parity build.

## Implementation requirement

1. Identify renderer-specific paths and the gameplay hook/gesture code in `src/game_xr.cpp` and its native module build definitions.
2. Compile the current gameplay code into **both** mode DLLs. Original must retain its original graphics path, and Revamped must retain its optimized graphics path. Do **not** just duplicate the Revamped DLL under the Original filename.
3. Stage both newly built DLLs in `build/windows-ninja/renderer-modes/{original,revamped}`. Never copy a previous release's Original DLL into a new gameplay preview.
4. Add a packaging/CI gate checking that both mode DLLs come from the current source revision and that gameplay integration tests run for both modes. Merely checking that both files exist or have different hashes is insufficient.
5. Validate the two modes in-headset for the same input sequences and mission interactions; compare rendering separately. No public release without owner approval.

## Branch / release safety

Work on `feature/0791-renderer-gameplay-parity`, based on the successful combined gameplay branch. Do not change the current #112 test artifact or public v0.0.79. The parity test ZIP must be separately named until both modes actually contain the new gameplay code.
