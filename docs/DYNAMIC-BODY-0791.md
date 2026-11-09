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
DYNAMIC distinguishes three wall states: **the game's native wall crawl**
(preserve the game's authored knee/ankle animation), **a HEROVR-owned slow
wall crawl or stationary cling** (procedural splayed legs with feet against
the surface), and **a fast HEROVR-owned wall run** (alternating gait with
feet/soles facing the wall, toes following actual sideways or upward velocity).
These are pose fallbacks, not recovered native Spider-Man run clips.

**Acrobatic flips:** the game tracking rig already turns Spider-Man through a
flip, but a rigid body rotating as one piece is not an acrobatic animation.
DYNAMIC now makes the spine curl and both knees draw upward near the middle
of an actual player flip, extending again toward level. This uses a dedicated
FlipMotion flag (not wall-body tilt or flight lean), works for ordinary
airborne jumps as well as swings, and never changes the headset view or
controller positions.

The user-facing toggle is in **HEROVR wrist hologram → VR SETTINGS →
BODY AND VIEW → BODY ANIMATIONS**. It switches CLASSIC/DYNAMIC live; the
older SM1 game-settings tab is not the user's control surface.

**Renderer requirement for this preview:** select **Revamped** in the
HERO VR launcher. The package deliberately preserves the official 0.0.79
**Original** renderer unchanged as a fallback; that Original DLL predates
this body feature and does not implement the new body setting.

- **CLASSIC** (default): the existing 0.0.79 solver.
- **DYNAMIC**: preserve actively moving game leg poses **and** a capped,
  smoothly animated hip/torso sway from Spider-Man's original animations.
  Leave the game's native wall-crawl animation on its legs when available.
  For HEROVR's own wall, keep a spider-like crouch when stationary, move the
  feet on a wall when slow crawling, or use a travel-direction-based running
  gait sideways or straight up. Add a spine-and-knee tuck during actual
  FlipMotion turns.

The setting is persisted as `leg_animations=0/1` in the existing compatibility
INI; the native pose solver is updated from the **HEROVR wrist hologram** live, without
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
2. Switch to DYNAMIC on the wrist holo's VR SETTINGS / BODY AND VIEW section,
   without restarting. Repeat the same
   movements, looking down to inspect thigh, knee and foot movement.
3. Check that both hands still aim and web independently, the headset does not
   bob, spin or follow canned head animations, and the legs do not snap when
   you stop or resume moving.
4. While airborne, use the **right stick** for FLIPS (the left stick stays
   movement) and REALISTIC FLIPS if enabled. Watch for a
   clear knee tuck and torso curl midway through the turn, then a smooth
   extension at the end. The headset and hands should continue tracking.
5. Test the game's **native crawl** (native legs should retain their pose),
   HEROVR's **stationary own-wall cling** (bent splayed legs and soles against
   the wall), HEROVR's **slow crawl**, and **fast sideways and vertical runs**
   separately. Ceiling/duct kneel must not trigger an inappropriate wall gait.
6. Switch back to CLASSIC, repeat the stationary wall pose and confirm that the
   older behavior returns immediately.
7. On the ground, hold the **left stick forward** and pump both tracked,
   empty hands in a real alternating running motion to request the game's
   RT Sprint/Parkour input (not L3, which is Dive). Let go or stop pumping to
   disengage. While airborne in the Sandman fight, press **Y** to request
   the game's native web-strike / web-zip to a target.
8. If the legs still look static, capture the pre-solve game pose (the existing
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

## Sandman release blocker

**Confirmed from the user's in-headset observation (November 9):** The defect
occurs in the **free-roam Sandman sand-cloud encounters where crystals are
collected**. The **entire cloud shape** appears to rotate as a single rigid
object when the player turns their head, like one flat cutout facing the viewer.
Its position remains fixed in the world: it does **not** travel with the HMD,
and it does **not** follow the player when looking away. Other objects/effects
do not exhibit this problem.

**Working hypothesis, not yet proven:** a view-facing *whole-effect billboard*
or cloud quad/mesh is receiving camera-orientation data from the stereo active
view. This is more specific than individual dust sprites turning independently
and does not imply a bad camera position. In `src/stereo_probe.cpp`,
`activeView()` replaces the stock active-view descriptor with the tracked
head pose (around lines 2518–2629); `placeEyes` also updates the world-facing
view later in the frame (around lines 3120–3171). Active-view globals are a
possible influence, but no cloud-specific draw pass, material, or orientation
write has been located. Do not blindly change the camera, culling, global
shader constants, all billboards, or world transforms.

**Required next investigation:** reproduce the crystal cloud in the same
location while turning the HMD in place; compare Revamped CLASSIC and DYNAMIC,
then Original versus Revamped using isolated installations. If possible,
capture the affected cloud in headset and desktop simultaneously. Identify
the **specific effect instance/material/draw call and its whole-cloud facing
transform** before designing a narrowly gated correction, with fallback and
regression checks for other effects and scenes.

The renderer/particle regression is **NOT** fixed or validated by these body
and input changes. Passing Windows CI does not prove that scripted Sandman
web-strike input works or that this cloud stays correctly oriented in VR.
**Keep PR draft and do not publish until headset verification.**
