# HERO VR 0.0.82.1 TEST 1 - HUD ANCHOR (private test, not a release)

Built from 0.0.81 (main 388ea99) on `experiment/082-hud-fix`. Only the game's UI layer cards changed
(health / focus / minimap pieces, world icons, waypoints, zip points, prompts). Scanner, camera, stealth, enemy grab,
weblines, rendering and performance are untouched. Both Classic and Performance get the same gameplay DLL.

## What was wrong (root cause)
The game draws its HUD into a flat layer from its own follow camera. 0.0.78-0.0.81 cut each lit piece out of that layer
and drew it at its flat-screen place in each eye's own view. So every piece, a world marker or a corner bar, was
**head-locked**: it turned with your head instead of staying over its target, it was off by however far the game camera's
look was from your head's, it was placed with a guessed 72 degree lens, and it swam under the headset runtime's late
reprojection. A piece's place also came straight from its latest 100 ms box, which jumps as the picture inside changes.

## What changed
* WORLD pieces (icons, waypoints, zip points, prompts): the piece's place on the layer is turned back into a ray from the
  game camera's pose and lens (the game's projection run backwards), anchored on that ray `hud_depth` metres out in the
  game's world, smoothed there, and drawn every frame from each eye's own view-projection. Over its target in both eyes,
  steady when you turn your head.
* HUD pieces (the ones at the layer's edges: health, focus, minimap, gadget bar): on a smooth panel 2 m in front of you
  that holds still while you look around within about 6 degrees and glides after a bigger turn; their place on the layer is
  smoothed, so no jitter.
* The game camera's own lens is used when it has the layer's shape (else the old 72 degree guess).

## Settings (live.txt, live, no restart; the holo has no page for these)
* `hud_anchor = auto` (default) / `world` (every piece anchored) / `head` (the old 0.0.81 head-locked cards).
* `hud_depth = 40` (5-300): metres out along a world icon's ray. Nearer helps prompts on close enemies, farther objectives.

## What to look at
1. Look at a waypoint / zip point / Y prompt and turn your head slowly: it must stay over the real thing in both eyes.
2. The health / minimap pieces must not fly around: they stay put through small head motion and glide back after a big turn.
3. Anything wrong: set `hud_anchor = head` and compare, then send the report. The log has one "HERO VR 82.1 HUD ANCHOR"
   line each 10 s: the mode, whether the game camera's lens was used, the camera's age, pieces and slots.
Never a full 2D flat screen in VR: only small cards cut from the layer, as before.
