# 0.0.74 test 4: TAA GATE LOG, EYE TAA REQUEUE, MAIN VIEW SMALL

Made by Agent Mulder.

## What test 3 showed

* The history weight is not stuck at 1.0. Test 2b's job data shows it near 0, and history is used.
* The game's TemporalAa ran for the eyes only part of the time (test 3, run 3: left 54%, right 13%).
  This may partly be a counting error, because test 3 matched each run only against the eye's newest job.
* The 67% eye size makes the image soft. The smear comes from about 46 fps on a 90 Hz headset, plus copy spikes.

## The three switches (TAA GATE LOG ON by default; EYE TAA REQUEUE and MAIN VIEW SMALL OFF by default since AA MODE CLEAN, docs/AA-MODE-CLEAN-074.md)

| live.txt key | Wrist row | What ON does | Restart |
|---|---|---|---|
| `taa_gate_log` | TAA GATE LOG | Every 10 s, four console lines under 256 characters: TemporalAa calls and runs per eye, jobs with no run, runs matched to an older job (from a 4-job ring per eye), other views' runs, and the history weight's inputs. OFF prints test 3's single line. | No |
| `eye_taa_requeue` | EYE TAA REQUEUE | When an eye has had 2 jobs in a row without a TemporalAa call, the game's TAA command (0xb1) is queued once more for that eye's view, once per job. | No |
| `main_view_small` | MAIN VIEW SMALL | Renders the game's own flat-screen view at 50% per side (multiples of 8) and lets its TAA resolve it, which leaves the eyes more GPU time. It works only when the game's Anti-Aliasing is TAA. | Yes (the next VR start) |

## Safeguards (nothing crashes or drops to the flat screen)

* EYE TAA REQUEUE runs inside an SEH guard. A fault turns it off for the session.
  It also turns off for the session after 8 requeues in a row that bring no run. Both print one console line.
* MAIN VIEW SMALL changes only the main view's own buffer sizes and the "render == output" size-flag bit (bit 2).
  It never uses view flag 32, and if the game's AA is not TAA it keeps the full size.

## Addresses (Spider-Man 2 2.810)

CONFIRMED:

| What | Address / offset |
|---|---|
| TemporalAa | 0x2c2dc60 |
| AA setting | [0xc2c6a10] |
| Job global | 0xc2c6af0 |
| Frame | 0xc2c6c2c / 0xc2c6c30 |
| Enqueue | 0x2c77810 (0xb1 on view+0x18d0, as at 0x2d017b3) |
| Main view | [0xc2ca220] |
| View buffer set | +0x1e38 |
| Set sizes | +0x1b978 |
| Job cut / cut frame / stamp | +0x19ab / +0x19ac / +0x1de8 |

CANDIDATE: TemporalAa runs for the eyes only part of the time. The test 4 log is there to confirm or refute this.
