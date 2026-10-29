# 0.0.73 TAA SHAKE

**Report (test 6):** with GAME TAA SPIDYVR on, medium and far buildings shake hard. GAME TAA OFF stops it (the owner CONFIRMED this).

## What was checked (exe disassembly, test 6 export)

- **CONFIRMED: the jitter is correct, as SpidyVR's.**
  - The lens `0x2c75560` stores `+0x428` = 2·x and `+0x42c` = −2·y (constant `0x53197c8` = −2.0). It builds the jittered projection as P × T(jx, jy), so the shift in clip space is the stored value.
  - TemporalAa `0x2c2dc60` takes back `0.5·[+0x428]·W` and `−0.5·[+0x42c]·H` pixels (constants `0x5303de8` = 0.5 and `0x53119d4` = −0.5).
  - Units and sign match. The flat camera uses the same scheme: ±0.875 pixel at 1280x720. Our eyes reached 0.874 pixel at 3496.
- **CONFIRMED: the previous camera TAA reprojects from is the eye's own** (12747/12747 in test 6's audit, view `+0x30` vs `+0x560`).
- **Strong CANDIDATE: W/H is the eye's own size.** The handler uses its set's `+0x3800`/`+0x3804`; `0x2d018d2` shows it is the set's `+0x3630` target's width. It falls back to its own target's size.
- **CONFIRMED code, CANDIDATE meaning: the current frame's weight.** It is max(1/(frame−`+0x19ac`+1), 1/(frame−`+0x1de8`+1)). It is 1 (no history at all) on the cut flag `+0x19ab` or when there is no history target.
  - CANDIDATE: under constant head motion the eyes' history is weak or reset, so the ±1 pixel sub-pixel jitter is not averaged away and reads as shake.
  - Test 6's own EYE history counted 1224 motion cuts in 11162 pairs.
- **CANDIDATE contributors:** headset sharpening 140% (the console's own note: it "sharpens the flicker of far roofs"), mip bias 0 (−1 at its most negative, 109 samplers), and supersample 130. SpidyVR renders the eyes at 67% and lets TAA upsample. That spreads its jitter over fewer, larger pixels.

## What changed (ON by default, revertible live)

- **TAA JITTER** on the wrist: FULL / HALF / OFF. The default is **HALF**.
  - FULL = test 6.
  - OFF keeps TAA running with no visible jitter.
  - live.txt `taa_jitter_scale` sets any percent.
- **Per-eye TAA record** `engine_taa_eyes` in the report. Per eye it has the stored jitter, the size TAA used, the pixels that makes, the history stamps, the current frame's weight, and how often the history was thrown away.
  - The next export answers the CANDIDATE: weight near 1 or many resets means the history is not holding.

## To try

1. HALF (default): is the shake gone or smaller?
2. OFF: still shaking? If yes, it is not the jitter. Look at sharpening and the history.
3. FULL: the test 6 look, for comparison.
4. Also try headset sharpen 0, supersample 100, and the headset at 90 Hz rather than 48 Hz.
