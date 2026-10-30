# MILES TAA (0.0.74 test 3)

`live.txt`: `miles_taa = on` (the default) or `off`. Wrist: COMBAT > MILES TAA.
Subtext: *ON runs Miles Morales VR's TAA in each eye at 67%; live, eye size at restart.*

The anti-aliasing MilesVR (the Miles Morales port of SpidyVR) uses, ported to Spider-Man 2 2.810.0.0:

| Step | What | Where (2.810) | Status |
|---|---|---|---|
| 1 | Halton (2, 3) lens jitter, the same for both eyes, phases = clamp(ceil(8 * ratio^2), 8, 32) | stereo_probe `eyeJitter` -> job jitter read by TemporalAa | CONFIRMED |
| 2 | Per-eye TAA targets (the eyes' buffers made with the TAA target kind) | `engineTaaBuffers`, mask table 0x5d65408 | CONFIRMED |
| 3 | Job AA (+0x570) = 1 for every eye job | copied to [0xc2c6a10], checked at TemporalAa 0x2c2dccb | CONFIRMED |
| 4 | Queue 0xb1 (TemporalAa) on view+0x18d0 when profile +0xcc8 bit 0 is set | SetupDisplay 0x2d016d0, site 0x2d017b3 | CONFIRMED |
| - | The eyes' scene at 67%, the game's TemporalAa resolving it to the eye's size | input set+0x3800, output target +0x1d0 | CANDIDATE (checked live, `upscaleOk`) |

While it is on: no EYE AA, no FXAA, no CAS sharpening, no TAA HOLD / TAA LINK.

## Proof, per eye (console, every 10 s)

TemporalAa 0x2c2dc60 (reads no arguments) is hooked for the log only. Each eye: `ran` = AA 1, a frame number,
a jitter, a target and the current slot stamped with F after the call (0x2c98240); `history` = ran, the other slot
F-1 and job+0x18e8 set (0x2c98290). The line prints F, the stamp, the other slot, +0x1a38, +0x18e8, +0x19ab and the
TAA input / output widths.

## Safety

* Every address above is byte-checked at the start (`include/spidy/miles_taa074.hpp`, tests/hero074t3_miles_taa_tests.py).
  A mismatch: one console line, SPIDYVR STOCK's path, eyes at 100%.
* The mask table or the game's Anti-Aliasing not making the targets when the eyes are made: the same.
* 45 left-eye jobs in a row with no eye pair reaching the headset: MILES TAA off for the session, one console line.
  Eyes already made at 67% keep their jitter and job AA (they need TemporalAa to reach full size); no black screen.
* The TemporalAa hook failing only loses the log.
* 30 runs where TemporalAa did not read the smaller scene: one console line (restart with `miles_taa = off`).

Made by Agent Mulder
