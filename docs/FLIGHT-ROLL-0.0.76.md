# HERO VR 0.0.76 - the flight's body roll, fixed (SM2)

Run flight-roll-1 (the owner's): flight was better, but rolled past about 90 degrees the camera got weird, at 180
(facing the clouds) the chest filled most of the view, it was laggy while rolling, and after rolling back it still
felt wrong until he landed.

## Why

1. **The axis swam.** The roll turned about the flight's velocity when faster than 2 m/s, otherwise about the head's
   level look - re-read every frame. Slow, it followed every head turn; around 2 m/s it jumped between the two; and
   the velocity only updates at the game's ~42 fps against the headset's 88 Hz, so the roll axis stepped.
2. **Only half the body rolled.** The suit body took its up from the rolled tracking turn, but its feet from the
   unrolled anchor. Rolled far over, the body's up and its feet disagreed, so the chest swung across the view (the
   multiplayer pose had the same mistake).
3. **Nothing reset.** Let go near level, a few degrees stayed on; landed, it levelled at 180 deg/s, not at once.
4. **The roll climbed.** The right stick is the roll (x) and the hover climb (y); a diagonal push did both (the log's
   "STICK 0.6 m/s (climb +0.6)" with the left stick at rest).

## Now (include/spidy/fly076.hpp, game_tracking, game_xr)

- `fly076::RollFrame`: the world axis is **locked when the roll starts** (the flight's way above 4 m/s, else the level
  look then) and only refreshed from fast flight; the roll is **one quaternion** about it (`Quat::around`), applied
  the **same frame** - no Euler angles, no smoothing.
- The head stays put; the body turns about it (`RollFrame::aboutHead`): the suit body's feet and the multiplayer pose
  now turn with the view, so the chest stays under the head in the rolled frame.
- Past 75 degrees the near plane steps out to 25 cm (back under 60 degrees: hysteresis, so the lens changes only on a
  crossing) - nothing of the chest can fill the view.
- `BodyRoll`: let go within 8 degrees of level, it is **exactly 0**; landed, it is 0 **the same frame**. At 0 the
  axis, the turn and the near plane are all cleared (`RollFrame::reset`).
- The right stick counts as the roll **or** the climb, whichever way it leans most (`rollStick` / `climbStick`).
- A log line a second while rolled: `VERSION 0.0.76 FLY ROLL: <deg> about (<axis>), near <m> m`, and one when level
  or landed.

Tests: tests/fly076_tests.cpp `rollFrameTests` (90, 180, 360 degrees, a 0-360 sweep with no jump, the return to 0,
the snap, landing, the stick split) and tests/hero076_roll_fix_tests.py (the wiring). Hover, strafe and climb are
unchanged apart from the stick split (the strafe moves by the yaw only, never the roll).

## Test 801ce57 follow-up: the view stays level (FLY ROLL COMFORT, the default)

The owner: "rotational flying is pretty screwed up still, the camera just gets really bad". Now, with live.txt
`fly_roll = 1` (COMFORT, the default) the headset view is **never rolled**: the right stick rolls only Spider-Man's /
the hero's body about the head (`fly076::rollBody`), and the near plane stays as it was. `fly_roll = 2` (FULL) is the
rolled view above, unchanged; `fly_roll = 0` turns the roll off. Each needs a restart (as before).
Tests: tests/fly076_tests.cpp `levelViewTests`.

## Aim flight (dive and climb where you look)

"You can't fly forward and down; you have to be hovering to go down." Now the left stick's forward share follows the
head's look in 3D in the new flight: look down while flying forward and you dive, look up and you climb; the strafe
stays level, and the right-stick climb / sink is kept. SUPERSONIC follows the look down too, but never into the ground
(on the ground a downward way is still levelled). Tests: tests/fly076_tests.cpp `aimTests`.
