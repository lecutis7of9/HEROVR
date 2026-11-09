# HERO VR 0.0.79.1 — Dynamic Body (experimental)

This is an **experimental test branch**, not a public release. It starts from the
0.0.79 code and deliberately leaves rendering modes, render scale, TAA,
headset pose and controller aim alone.

## The change

In the game's pose-writer hook, the original Spider-Man leg pose arrives before
the VR solver moves the body under the headset. Previously, the feet were
always planted on walls or ceilings (and ordinary grounded feet were partially
anchored). This branch observes each game's knee and ankle movement **relative
to its own hips**, so moving Spider-Man through the world alone cannot be
mistaken for an animated stride. When the game changes these lower-body
joints, it blends the planted-foot constraint out and retains the game's
animated pose. When motion stops, it blends the constraint back in.

**Also new: animated hip and torso follow-through.** The original game
pose's hip up-axis and hip-to-head direction are read before VR IK. When they
change, HERO VR carries a small, smoothed amount of their lean and sway into
the pelvis and spine aligned to the player's body direction. Hips are limited
to 0.12 rad (~7 degrees), and the upper torso to 0.20 rad (~11 degrees);
they settle back to neutral when animation stops. This affects the avatar
only: the HMD stays at its independently tracked viewpoint and both wrists
are still solved to their motion controllers after the body adjusts.

**Following the November 9 headset recordings:** HEROVR's own wall movement
sometimes leaves the Spider-Man 2 actor in an AIR state with tucked knees.
DYNAMIC now synthesizes a short alternating leg stride driven by the real
tangential wall-running speed, but only while moving on a HEROVR-owned wall.
Stopped wall poses retain the existing fallback. This is a procedural
footstep gait, not a newly discovered native Spider-Man running clip.

**Acrobatic flips:** the game tracking rig already turns Spider-Man through a
flip, but a rigid body rotating as one piece is not an acrobatic animation.
DYNAMIC now makes the spine curl and both knees draw upward near the middle
of an actual player flip, extending again toward level. This uses a dedicated
FlipMotion flag (not wall-body tilt or flight lean), works for ordinary
airborne jumps as well as swings, and never changes the headset view or
controller positions.

The menu path is **Settings > SPIDY VR > BODY > BODY ANIMATIONS**.

**Renderer requirement for this preview:** select **Revamped** in the
HERO VR launcher. The package deliberately preserves the official 0.0.79
**Original** renderer unchanged as a fallback; that Original DLL predates
this body feature and does not implement the new body setting.

- **CLASSIC** (default): the existing 0.0.79 solver.
- **DYNAMIC**: preserve actively moving game leg poses **and** a capped,
  smoothly animated hip/torso sway from Spider-Man's original animations.
  Keep planted feet in stationary or static-crawl wall states, where the
  game's pose is not moving. Add wall-speed-driven running legs if HEROVR
  controls wall movement but the game supplies static AIR legs; add a
  spine-and-knee tuck during actual FlipMotion turns.

The setting is persisted as `leg_animations=0/1` alongside the other tab-only
options; the native pose solver is updated when the menu setting changes, without
a camera/headset or arm-controller mode change. The 0.0.79 native command
structure and status wire layout remain unchanged.

**Important limitation:** this does not discover or trigger Spider-Man 2's
native animation graph. The wall stride and flip tuck are safe procedural
approximations layered over the game's available pose. A fully authored
Spider-Man swing/run animation still requires identifying its engine state
and replaying or restoring that source animation without taking over the
VR-driven head or arms.

## Test plan

1. Launch a private Windows test build, load normal Spider-Man free roam and
   choose CLASSIC. Look down while stationary on the ground and wall, walking,
   wall-running, swinging, and vaulting. This is the baseline.
2. Switch to DYNAMIC in the BODY menu, without restarting. Repeat the same
   movements, looking down to inspect thigh, knee and foot movement.
3. Check that both hands still aim and web independently, the headset does not
   bob, spin or follow canned head animations, and the legs do not snap when
   you stop or resume moving.
4. While airborne, use FLIPS and REALISTIC FLIPS if enabled. Watch for a
   clear knee tuck and torso curl midway through the turn, then a smooth
   extension at the end. The headset and hands should continue tracking.
5. Repeat a stationary wall cling/ceiling and a low-ceiling kneel: the fallback
   should still plant the legs, not impose a static splayed crawl pose.
6. Switch back to CLASSIC, repeat the stationary wall pose and confirm that the
   older behavior returns immediately.
7. If the legs still look static, capture the pre-solve game pose (the existing
   `SpidyBodyCapture` / `SpidyBodyPoses.game` diagnostic) during the movement
   in question. A static game pose is the blocker, not a missing IK setting.

## Safety, build, and status

- The existing `tests/game_body_tests.cpp` has synthetic moving-leg and
  airborne hip/torso-sway, real-speed wall stride, and tracked flip-tuck
  regressions comparing CLASSIC and DYNAMIC,
  including fixed head and independently controller-driven wrist positions,
  and an idle-sway fallback.
- This branch uses a separately named **0.0.79.1 Dynamic Body TEST** ZIP.
  The old in-game test notice is **disabled** because its hardcoded fallback
  instructs players to revert to 0.0.73; the correct baseline for this
  experiment is the public **0.0.79** release.
- The branch has an isolated Windows Actions workflow
  `.github/workflows/dynamic-body-0791.yml`. On a passing build it attaches a
  test mod ZIP to that workflow run as an **Actions artifact**, never to the
  public GitHub release. No release is published by that workflow.
- Build/test status must be read from GitHub Actions. A source commit or a
  configured CI workflow **does not prove** the game was tested in-headset.
- Run a private test build in a separate folder; do not overwrite or publish
  the known-good 0.0.79 launcher/mod folder.
