# HERO VR 0.0.79.1 — ONE combined gameplay test, separate story backlog

Owner direction (November 9, 2026): do **not** require a restart or a
different build for each web, zip, sprint, body or stealth input. Combine
those changes in a single private 0.0.79.1 test ZIP. Mission-specific
compatibility may be fixed separately, and must not hold up the generally
working gameplay changes.

## Track A: combined gameplay preview (one install, one headset session)

The existing `.github/workflows/dynamic-body-0791.yml` builds ONE
`HERO-VR-79.1-TEST-win64.zip` and uploads one GitHub Actions artifact
`herovr-79-1-combined-gameplay-test`. This remains a **private experiment**:
neither an Actions artifact nor passing automated tests authorize publishing.

Run on the same Spider-Man 2 save with the **Revamped** renderer selected.
In a single launch, check the following in this order:

1. **Regression baseline:** both hands track and aim normally; ordinary grip
   web-swing and release work. Repeat after each new action.
2. **Ground sprint:** left stick forward with alternating empty arm pumps sends
   game's native sprint/parkour; stops when pumping stops. Confirm no L3 dive.
3. **Enemy grip split:** with one free hand aimed at a tracked enemy, squeeze
   grip and hold briefly (0.24 s) to request the game's native web-strike.
   With both hands forward, squeeze both grips within 0.24 s, then pull to
   run the existing GrabWeb yank. Confirm two grips NEVER trigger a zip.
   Grips with no enemy continue normal web-swinging. Y is no longer an
   unprompted airborne enemy-attack shortcut; mission prompts retain Y.
   Confirm Sandman scripted web-strike with grip separately.
4. **Web Line:** on valid ground/perch aim with left trigger and tap Y; check
   bridge placement, walking/standing on it and no collision with the existing
   airborne Y action.
5. **Stealth:** while on a native Web Line, use the game's displayed Square/X
   or Triangle/Y stealth prompt against an eligible enemy; verify native
   takedown and no unintended menu/punch or web strike.
6. **Environmental grip-yank:** with game's L1+R1 prop prompt active, grip while
   aimed and retract hand deliberately; confirm game's native yank/throw.
   Away from a prompt, gripping must continue to swing normally. This does NOT
   promise a physics-based object held on a separate web rope.
7. **Surge:** when ability is unlocked/charged, L3+R3 triggers native Surge,
   does not pause or enter PAD; hold BOTH triggers + L3+R3 to toggle PAD and
   ensure it does not trigger Surge.
8. **Body / acrobatics:** CLASSIC versus DYNAMIC from HEROVR wrist holo;
   original moving game legs preserved; slow wall cling/crawl, horizontal and
   vertical wall runs show feet on wall; right stick drives flip and knee tuck;
   left stick retains locomotion; HMD and wrists never inherit body sway.
9. **Render regressions:** original and Revamped toggle still work; report
   Sandman cloud billboard rotation separately, do not silently declare fixed.

Mark each as PASS / FAIL / NOT TESTED with the location, save, mode and
a short headset recording or report where practical. No per-feature reboot:
switch BODY ANIMATIONS inside headset and perform all gameplay scenarios on
this install. Mission objectives requiring different saves can be tested later.

### Track A release gate

A public release candidate is possible **only after** Windows CI/package
passes, the combined general-control tests are verified inside the headset,
no major regression appears, and the owner authorizes release. Failures of
Photo Ops, Prowler stash or the Midtown school sequence by themselves are
**not** vetoes for Track A. Document them as known story limitations.

**Separate existing concern:** the Sandman cloud rotating toward the headset
remains an unresolved renderer issue. It needs an explicit owner decision
about release scope/risk; do not silently treat it as fixed or waive it.

## Track B: story-specific compatibility (separate work/release gate)

- **Photo Ops:** test native pause-menu Open Camera Gadget and optional
  user-configured D-pad Toggle Camera Gadget shortcut; the real Photo Op
  focus and shutter must advance the quest, not Photo Mode. A direct one-step
  VR gesture requires verified game-side shortcut behavior before coding it.
- **Prowler stashes:** preserve independent proportional L2/R2 values in PAD
  for yellow pressure zones; verify scan, two holds, native L1+R1 opening and
  mission flag in headset. Do not replace analog values with binary holds.
- **Roll Like We Used To:** native Peter/Harry bicycle and native Young Peter
  body/cinematics; test doorway, vent and window traversal. Correlate
  sideways movement with `headPushFrames`/`headPushMostMm` before narrowly
  adjusting HeadClearance; no global wall-collision bypass.

Any Track B change that touches core controller or tracking code must pass
Track A regressions. A Track B fix may use a later mission-specific preview
and must not retroactively hold the working Track A gameplay package.

## Status and limits

This document and the renamed combined preview workflow do not themselves
change game inputs, establish CI success, create a public release or verify
headset operation. The single private ZIP is only available if its Windows
GitHub Actions workflow runs successfully. Keep public 0.0.79 unchanged.
