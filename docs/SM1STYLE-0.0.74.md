# SM1STYLE (0.0.74 test 10)

SpidyVR's own eye anti-aliasing, ported to Spider-Man 2 and ON by default. The game's TAA runs in each eye, the way
SpidyVR does it on Spider-Man 1:

- **Jitter.** Halton (2, 3) lens jitter, the same for both eyes: `2 * (h - 0.5) / render size`, with
  `clamp(ceil(8 r^2), 8, 32)` phases. SpidyVR: `src/stereo_probe.cpp:1294-1306`.
- **TAA targets.** The eye buffers are made with the TAA targets. SpidyVR: `src/stereo_probe.cpp:917-932, 961-990`.
- **TAA forced on.** The eye job's AA is set to TAA (settings +0x570 = 1). SpidyVR: `src/stereo_probe.cpp:1045`.
- **TemporalAa queued.** TemporalAa (0xb1) is queued for the eye views. SpidyVR: `src/stereo_probe.cpp:906`.
- **Eyes at 67%.** The eyes render at 67% per side, and the game's TAA resolves them to full size. SpidyVR:
  `include/spidy/eye_resolution.hpp:25` and `tools/run_game_vr.py:52`.
- **Render scale 100.** SpidyVR: `src/game_xr.cpp:112`.

While SM1STYLE runs, none of our own eye passes touch the picture:

- FXAA / downsample, EYE AA history, CAS sharpen, ENGINE TAA and the EYE AA lens jitter are all off. This also
  covers JITTER CALM and LIGHT HOLD.
- The wrist rows AA, HISTORY, SHARPEN and SUPERSAMPLE show UNAVAILABLE.
- These are ignored:
  - the live.txt keys `aa`, `aa_quality`, `sharpen`, `history`, `history_cut`, `eye_temporal` and `taa_jitter`
  - the launcher arguments `-RenderScale`, `-Supersample`, `-MipBias`, `-Anisotropy`, `-GameSharpening`,
    `-KeepGameSettings` (sharpening only) and `-EngineTaa`
  - next-launch.txt

  Their values stay saved for `sm1style = off`.
- The game's mip bias and filtering are kept as they are, the game's sharpening is set to its lowest, and eye bloom
  is off.

**Off switch.** Put `sm1style = off` in live.txt and restart VR: you get the 0.0.72 eye picture again.

**Safety.** SM1STYLE checks itself and falls back to the 0.0.72 path (with one console line) in these cases:

- Any checked byte of the game's TAA code, or its buffer mask table, is not the 2.810 exe's.
- The game's Anti-Aliasing is not TAA / DLAA.
- 45 eye jobs pass with no pair reaching the headset.

If the TemporalAa logging hook fails, SM1STYLE keeps running; only the logging is lost.
