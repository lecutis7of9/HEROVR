# HERO VR — SpidyVR renderer-parity / FPS investigation (2026-11-10)

**Separate experimental work. No runtime changes, no public release, no claimed FPS improvement.**
Base: public HERO VR v0.0.79. Do not alter 0.0.79.1's gameplay candidate or the window-light visibility experiment.

## Verified architecture comparison

| Subject | IlyaMez/SpidyVR (Spider-Man Remastered) | HEROVR (Spider-Man 2 2.810) | Implication |
| --- | --- | --- | --- |
| Scene rendering | Two real native offscreen camera views, one per eye | Two native scene views already ported in `src/stereo_probe.cpp` | Do **not** rewrite the stereo renderer. Native stereo is already the core. |
| Eye resolution / AA | Headset-size outputs; 67%-per-side scene rendering using the game's own TAA and temporal reconstruction (SpidyVR `docs/DEVELOPMENT.md`, `src/stereo_probe.cpp`) | v0.0.79 Revamped uses 70%-per-side per-eye image and its own full-size temporal reconstruction in `src/native_eye_gpu.cpp`; Original preserves v0.0.78 renderer | Main opportunity: verify the cheaper, engine-native eye TAA path or reduce per-eye shading cost while preserving clarity. |
| SM1-like eye TAA | Engine owns eye TAA targets/history and subpixel jitter | `include/spidy/sm1style.hpp` already ports most of this; `tools/run_game_vr.py` exposes `vr_aa=game` and `vr_render=upscaled` at 67%, BUT this is not the v0.0.79 Revamped 70% pipeline | Audit actual SM2 per-eye input size, output target size, history ownership, TemporalAa runs, and frame time. No benefit should be claimed merely from a setting or diagnostic readout. |
| Main monitor scene | In immersive VR, upstream tries to rest the stock monitor render and mirror an eye, saving a third full scene render | The `restGameView()` mechanism is ported (SM2 active-view readiness `+0x13f8`); `SM2_STEREO_RESTED_MARKS=0` means downstream drawn-instance marking is *not* ported | Count whether stock view really has a render job when immersive and whether disabling it disrupts streaming, window LOD, HUD, mission prompts or shadows. |
| Occlusion | Offscreen eyes acquire depth occlusion objects (`giveOcclusion`) | Same helper exists, created when `config.createViews & 16` | Verify both eyes actually have occlusion. If already enabled, do not redundantly implement. |
| Eye shadow reuse | Conditional first-eye donor cascades sampled by second eye | Ported hook in `submitScene`, controlled by `eyeShadows` bits | Measure GPU milliseconds and inspect disocclusion, side-eye shadow mismatch, indoor/night lights before enabling/reusing more work. |
| Rendering workload | Profiler tracks unique stereo pairs, desktop cost, per-eye GPU time, CPU threads | `tools/run_game_vr.py` and `probe_stereo_gpu.py` expose unique pairs, eye GPU events, CPU/system information | Measure actual new game stereo pairs rather than headset/compositor 40/45 FPS, which can be a reprojection ratio. |

## Practical priorities (smallest safe changes first)

1. **Use evidence already collected.** First compare the same-save Original vs Revamped reports (renderer choice, exact width/height *per eye*, render scale, actual GPU ms/eye, unique game-generated frames, headset refresh, reprojection/SSW state, CPU main/render-thread time). No need to reboot per sub-feature.
2. **Confirm default work distribution.** In the code and one *instrumented* game session, check whether stock-monitor rendering is actually rested (`config.createViews & 32`), per-eye occlusion is on (`& 16`), and the game really emits TWO eye scenes, not three. Existing offset `+0x13f8` and empty drawn tracking require caution.
3. **Verify native TAA on SM2 without replacing Revamped.** In an isolated renderer candidate, use the existing SM1STYLE route at 67%, but accept it only if every eye has private TAA targets/history and the engine's output full resolution is verified (see `sm1style::upscaleOk`); compare GPU cost, moving edges/ghosts, buildings and night windows. Fall back automatically if validation fails.
4. **Tune existing Revamped first.** Current 70% per-axis means approximately 49% as many per-eye shading pixels as native 100%. 67% is ~44.9% (about 8.4% fewer scene pixels than 70%), but this does *not* promise 8.4% higher FPS; postprocessing and CPU costs may dominate. Preserve independent output size and pre-existing AA/history and ability to fall back.
5. **Then profile shadow/culling overhead.** Prefer to fix missing eye occlusion, duplicate main-scene work, duplicated cascade jobs or avoidable GPU copies over adding a fourth renderer. Preserve view-owned visibility queries: a previous cross-eye distance-field write made building windows flicker.
6. **Only then** consider adaptive per-eye scale / optional high-performance preset. A too-aggressive scale jump or invalidated history causes eye shimmer, worse windows and stereo mismatch; change resolution only at a safe life-cycle boundary and retain the shipped Revamped option.

## Acceptance gate

- Never merge a renderer experiment into the private gameplay test branch until the gameplay build passes CI *and* the user has tested movement/combat.
- Performance experiment must be on its own branch, with a hard OFF/fallback, same-save and same-camera comparisons, and no public release until user approval.
- Measure actual median/1% low (or p95) frame time, not only a headset overlay's half-refresh rate. Record headset model, runtime and Hz, VR per-eye size, render scale, Original/Revamped, game graphics quality, GPU/CPU model and GPU memory usage.
- Validate physical headset head movement, no left/right mismatch, stable night windows (turn head), Sandman clouds, menus/cutscenes and FPS after ≥1 minute. These existing bugs are not presumed fixed.
- A candidate that merely improves FPS by making a surface disappear, skipping the game's work, or increasing window flicker fails.

## Source pointers

- SpidyVR source: https://github.com/IlyaMez/SpidyVR/blob/main/src/stereo_probe.cpp
- SpidyVR render notes: https://github.com/IlyaMez/SpidyVR/blob/main/docs/DEVELOPMENT.md
- HEROVR stereo: https://github.com/lecutis7of9/HEROVR/blob/v0.0.79/src/stereo_probe.cpp
- HEROVR 70% temporal upscale: https://github.com/lecutis7of9/HEROVR/blob/v0.0.79/src/native_eye_gpu.cpp
- HEROVR SM1-style TAA: https://github.com/lecutis7of9/HEROVR/blob/v0.0.79/include/spidy/sm1style.hpp
- HEROVR Revamped release: https://github.com/lecutis7of9/HEROVR/releases/tag/v0.0.79

**Current outcome:** Code/research audit only. No FPS gain measured; no native renderer or headset tested in this task.
