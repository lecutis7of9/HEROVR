# HERO VR 0.0.79.1 — Dynamic Body (experimental)

This is a **private test branch**, not a public release. It starts from the
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

The menu path is **Settings > SPIDY VR > BODY > LEG ANIMATIONS**:

- **CLASSIC** (default): the existing 0.0.79 solver.
- **DYNAMIC**: preserve actively moving game leg poses. Keep planted feet in
  stationary or static-crawl wall states, where the game's pose is not moving.

The setting is persisted as `leg_animations=0/1` alongside the other tab-only
options; the native pose solver is updated when the menu setting changes, without
a camera/headset or arm-controller mode change. The 0.0.79 native command
structure and status wire layout remain unchanged.

**Important limitation:** this is pose reuse, **not a replacement animation
player**. If Spider-Man 2 supplies an idle pose during manual web swinging,
or a crawl rather than a run during wall movement, the mod cannot turn that
pose into an authored swing or running animation merely by freeing the legs.
The next step in that case is to capture the pre-VR pose, determine the game's
underlying movement/animation state, then selectively drive the proper
animation without handing camera and arms back to the game.

## Test plan

1. Launch a private Windows test build, load normal Spider-Man free roam and
   choose CLASSIC. Look down while stationary on the ground and wall, walking,
   wall-running, swinging, and vaulting. This is the baseline.
2. Switch to DYNAMIC in the BODY menu, without restarting. Repeat the same
   movements, looking down to inspect thigh, knee and foot movement.
3. Check that both hands still aim and web independently, the headset does not
   bob, spin or follow canned head animations, and the legs do not snap when
   you stop or resume moving.
4. Repeat a stationary wall cling/ceiling and a low-ceiling kneel: the fallback
   should still plant the legs, not impose a static splayed crawl pose.
5. Switch back to CLASSIC, repeat the stationary wall pose and confirm that the
   older behavior returns immediately.
6. If the legs still look static, capture the pre-solve game pose (the existing
   `SpidyBodyCapture` / `SpidyBodyPoses.game` diagnostic) during the movement
   in question. A static game pose is the blocker, not a missing IK setting.

## Safety, build, and status

- The existing `tests/game_body_tests.cpp` has a synthetic moving-leg
  regression that compares CLASSIC and DYNAMIC and checks head and hand
  positions remain identical.
- This branch enables the **test-build** notice (0.0.79.1 TEST 1) rather than
  claiming to be a new public version or modifying the existing release.
- The branch has an isolated Windows Actions workflow
  `.github/workflows/dynamic-body-0791.yml`. On a passing build it attaches a
  test mod ZIP to that workflow run as an **Actions artifact**, never to the
  public GitHub release. No release is published by that workflow.
- Build/test status must be read from GitHub Actions. A source commit or a
  configured CI workflow **does not prove** the game was tested in-headset.
- Run a private test build in a separate folder; do not overwrite or publish
  the known-good 0.0.79 launcher/mod folder.
