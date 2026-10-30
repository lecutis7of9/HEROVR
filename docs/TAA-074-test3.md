# 0.0.74 test 3 - why the buildings still shake (TAA)

Made by Agent Mulder. Evidence: the test 2b headset report (session game-vr-20261029-230151, 8908 samples),
its two video clips (head held still for about 7 s), and Spider-Man2.exe disassembly.
CONFIRMED means read from the exe or the report. CANDIDATE means the evidence points to it but it is not proven.

## Short answer

* **TAA HOLD never fired** (CONFIRMED). The report says jobs 4550 and held 0. The stamps it waited for
  (both ages 0 on two jobs in a row) never happened: the job stamp age kept growing (last 8311) and the cut age too (4656).
  So TAA HOLD did nothing, good or bad. **It is now OFF by default** (still switchable, code 5504; now 0 = OFF, 1 = ON).
* **"Objectively worse"** comes from ENGINE TAA's jitter shown without a working history (CANDIDATE, strong).
  The eye views get a new sub-pixel jitter every frame. TemporalAa only averages it away when it accepts the eye's
  history. When it refuses the history, the jittered frame is shown as is, and every high-contrast edge moves by up to
  half a pixel each frame. That is the shimmer the owner saw with a still head.
* The real history test was never looked at by TAA HOLD or by the test 2b diagnostics (CONFIRMED from the exe).
  See below.

## How TemporalAa decides (CONFIRMED, Spider-Man2.exe)

| address | what it does |
|---|---|
| 0x2c2dc60 | TemporalAa. It reads the job through 0xc2c6af0 and the jitter at job+0x428 / +0x42c. |
| 0x2c2dd09 | If the jitter is zero, TemporalAa skips the whole pass. |
| 0x2c2dd13 | Frame F = [0xc2c6c2c]. The job copy fills it at 0x2c81958 from 0x2c91fe0, which returns global 0xc2c9d40. |
| 0x2c98240 | Current target. It writes F into *[job + 0x1a28 + 8*[job+0x1a38]]. |
| 0x2c98290 | History target. It exists **only if** *[job + 0x1a28 + 8*(1-[job+0x1a38])] == F-1 and the previous set job+0x18e8 has its buffer. |
| 0x2c2e1ee..0x2c2e218 | No history target, or the cut flag job+0x19ab is set: the current frame's weight is 1 (no accumulation). Otherwise the weight is max(1/(F2-cutFrame+1), 1/(F2-stamp+1)). |
| 0x2c96f80 | The view update. It flips +0x1a38 (and +0x1a58 .. +0x1ad8) and sets +0x18e8 = +0x18e0. It is called from 0x2a862e4, 0x2c77d66 and a loop at 0x2c9c7f1..0x2c9ca26. |

The history therefore needs the eye view's slot index to flip exactly once per rendered frame, and the slot it
reads to have been stamped by that eye's previous frame. Test 2b's TAA HOLD and TAA DIAGNOSTICS only modelled the
second half (the cut/stamp weight), so they could not see this gate.

## What the headset evidence says

* ENGINE TAA was live for only 1905 of the 8908 samples, from 22.6 s to 126.2 s (CONFIRMED). For the rest, the eye
  jitter stayed frozen at [0.000279, 0.0000386] and the game's TAA was off. It did not come back after the menus
  (CANDIDATE: the auto retry only runs while engineTaaAuto is set).
* Clip 1 is a still head at 45.5 s (CANDIDATE time match: head at 638, 9.1, 883, looking slightly down), with ENGINE TAA ON.
  Frame differencing of the still segment gives:
  * a smooth drift of about 0.4-0.6 px per frame (the head);
  * a high-pass residual of 0.22-0.29 px rms on the distant buildings, which never settles;
  * temporal-std heat on every high-contrast edge and lit window.
  A working TAA converges in about 8-16 frames, so a residual that never shrinks means the history is refused.
* The test 2b diag read weight 1.0 with cut and stamp ages 0, and 408 history-reset samples. Those were read from
  the VIEW and miss the gate, so they do not prove which part refused the history.
* Jitter values and eye cameras are the same as SpidyVR (CONFIRMED, source compare). The difference is that SpidyVR's
  first stereo build ran the AA on the monitor image only, so it never showed un-resolved jitter in the eyes.
* Other contributors (CANDIDATE):
  * IMAGE classic FXAA plus sharpen was on (CONFIRMED toggled) and amplifies the shimmer.
  * The launch mip bias of 0.5 softens near textures ("near street textures look soft").
  * 44 fps on a 90 Hz headset (CONFIRMED) halves how fast any history converges.

## Possible causes of the refusal (CANDIDATE, test 3 measures which)

1. The eye view's +0x1a38 is not flipped exactly once per rendered frame (0 or 2 calls to 0x2c96f80). The history
   slot is then never re-stamped, or is stamped F rather than F-1.
2. The eye's slot pointers +0x1a28 / +0x1a30 are shared with the main view (cloned), so the main view's stamps
   overwrite the eye's.
3. The eye's previous set +0x18e8 is null.

## What test 3 ships

* **TAA HOLD: OFF by default** (switchable). It never fired.
* **TAA LINK** (read only, always on while ENGINE TAA is live). It records, per eye job after the game's copy:
  * the slot index;
  * whether the history slot and the previous set exist;
  * how far the frame (0xc2c9d40) is past the history stamp, in buckets 0, 1, 2 and 3+;
  * how many jobs the slot stayed unmoved.

  It is exported as SpidyTaaLinkData. The console prints "TAA LINK: L refused x% (age 0/1/2/3+) ..." every 10 s, and
  the report has `taa_link` in each sample. The next headset report answers which cause above it is.
* **JITTER GUARD: ON by default** (switchable, code 5506, live). When an eye's history slot has not moved for 8 jobs,
  or is missing, the game can never accept that history: the stamp cannot equal F-1, so this test is exact. The eyes
  then get a constant near-zero jitter (1e-6 NDC, about 0.001 px) instead of the Halton jitter. TemporalAa keeps
  running, because the jitter is not zero (0x2c2dd09), so the gate is still checked every frame. Nothing shimmers.
  As soon as both slots move again, the full jitter is back.

  Without a history, the picture is simply the un-jittered frame: stable with a still head, no shimmer. When the
  history works, the guard does nothing. This is the only default-on change: it removes the one source of
  frame-to-frame motion in a still scene that the exe proves goes un-resolved.
* Not changed (no evidence yet): the per-eye own-slot fix (cause 2), the ENGINE TAA retry after menus, FXAA and
  sharpen defaults, and the mip bias. All of them wait for the TAA LINK numbers.

## What to send back

The console's `TAA LINK` lines, or the report JSON (`taa_link`), with a still head looking at distant buildings,
taken twice: once with JITTER GUARD ON and once OFF.
* "refused" near 100% with ages at 3+ means cause 1 or 3.
* Ages at 0 mean the slot is stamped in the same frame, which is cause 2.
* "refused" near 0% with the shake still there means the history works and the shake is elsewhere.
