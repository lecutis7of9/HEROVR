# 0.0.74 test 2b - TAA in the headset (findings)

Made by Agent Mulder. CONFIRMED = seen in the code, the exe's disassembly or the test 2 report.
CANDIDATE = our best reading, still to be shown by the next test's numbers.

## Does the game's AA run on each eye, or only on the monitor view?

- CONFIRMED (code): the eyes get the game's AA only while ENGINE TAA is live. Without it the bridge copies
  eye images that never went through the game's TAA pass - the same fault SpidyVR's creator first had (AA on
  the monitor view only). The launcher turns -EngineTaa ON by default, forces the game's AA to TAA with no
  upscaler, gives each eye its own TAA targets and history buffers, and sets the job's TAA flag (job+0x570).
- CONFIRMED (SpidyVR source): SpidyVR fixes it the same way - its CHANGELOG's per-eye temporal state
  (eyeTemporal), ON by default.
- CONFIRMED (test 2 report): AA = TAA, upscale None; ENGINE TAA ran 15394 frames with targets L1 R1; each eye
  reprojected from its own previous camera (previous_own 10101/10101); jitter FULL, the same in both eyes. With
  ENGINE TAA off the eyes had no AA ("too sharp").
- CONFIRMED (exe, copyJob 0x2d03240): the TAA job is a fresh 0x1e20-byte copy of the view each frame
  (alloc 0x1e20, memcpy(job, view, 0x1e20), stored at context+0x650), so writing the job never touches the view.

## Why it still shakes

- CANDIDATE (strong): the TAA handler (0x2c2dc60) weighs the history max(1/(F-cutFrame+1), 1/(F-stamp+1)).
  The report's history weight read 1.0 at every readback (ages 0, 1451 reset samples): each eye threw its
  history away every frame, so TAA only jittered - the shake - and never settled.
- CANDIDATE: the frame number is read at context+0x790.

## The fix: TAA HOLD (ON by default; "taa_hold", OFF switches it off live, no restart)

On each eye's own job copy only: when that eye's job came stamped "reset this frame" twice in a row (both stamps
equal the frame), with no camera cut and the eye moved under 1 m, both stamps are set 16 frames back so the
handler blends that eye's own history. A real cut (job+0x19ab) or a jump of 1 m or more is left alone. If the
frame read is wrong the stamps never match and nothing is written. The SpidyTaaHoldData record (THLD v1) counts
jobs, holds, zero ages and cuts per eye; run_game_vr.py logs it every 10 s so the next test confirms or rules
out the cause.
