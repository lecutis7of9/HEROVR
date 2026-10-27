# HERO VR 0.0.7: Batman, Superman, Punisher, Wolverine and Captain America

HERO VR 0.0.7 is a public release (version number **0.0.70**, tag v0.0.70: versions compare number by number, so
the trailing 0 keeps it above 0.0.63 for every launcher), built on 0.0.62/0.0.63. Every new
option is OFF, or keeps its 0.0.62 behaviour, until the player turns it on. All the options are in
the wrist HUD's COMBAT page under HERO PROFILES, and every option is live.

## Smoother cutscene starts (performance)

A scene's 3D eye views (GAME SCREEN 3D) are shown only with the HUD panel on their screen, and that needs the HUD
panel's placement hook, which the SM2 port does not have (`SM2_STEREO_HUD 0`). Until 0.0.63 the mod still asked the
game for the two eye views for about 1-1.25 s at every cutscene start or resume, and then threw them away: two full
headset-size views on top of the game's own, a hitch at each cutscene start. 0.0.7 asks only when the hook is
installed (`sceneStereoAsked`, `native_hud::panelHooked`), so cutscenes stay on the flat screen exactly as before,
without the hidden work. GAME SCREEN 3D's text says that cutscenes stay flat in SM2 for now. Tests:
core_tests ("the game screen is in stereo only while the game plays a scene") and tests/scene_stereo_gate_tests.py.

## Settings (combat38 options 54-70)

| Option | Values (default first) | What it does |
|---|---|---|
| BATMAN / WOLVERINE / SUPERMAN / PUNISHER | OFF, ON | Hero profiles. Only one hero can be on at a time, including the 0.0.6 heroes. |
| BATARANG CHAIN | 25%, 50%, 75%, 100%, OFF | Each hit rolls to bounce to another enemy within 5 m. A batarang hits 3 enemies at most, then comes back. |
| EASY RELOAD | AUTO, REALISTIC | AUTO refills an empty gun. REALISTIC: flick the gun down, then touch it to your hip. |
| HOLSTER HANDS | SAME SIDE, EITHER HAND | Which hand may draw each hip pistol. Either hand can draw from the back. |
| BATON HOLSTERS | OFF, ON | Daredevil's batons ride on the back. To draw one, grip over a shoulder. |
| BATON LOOK | CLASSIC, CLUB, MODEL | MODEL shows Anthony Yanez's batons, falling back to CLUB if the files are missing. |
| BATARANG STYLE | DEFAULT, EIRA, CLOONEY, JOKER | A drop-in model. A missing or broken file draws DEFAULT. |
| GUN MODELS | BUILT-IN, MODELS | Punisher's guns as the converted models, or BUILT-IN if the files are missing. |
| CAPTAIN AMERICA ABILITIES | OFF, ON | His shield on your back; draw, guard and throw it (one hero at a time). |
| SHIELD ARM | RIGHT, LEFT | The forearm that wears the shield (RIGHT by default); only that hand draws it from the back. |
| SHIELD DAMAGE | 35 HP, 20 HP, 50 HP, 70 HP | Each thrown hit, a real blow on the CONFIRMED sm2_damage path. |
| SHIELD CHAIN | ON, OFF | ON: ricochets to two more enemies within 5 m (3 at most, like the batons), then home. |
| SHIELD BLOCK | OFF, EXPERIMENTAL | CANDIDATE: a hit while the shield faces the nearest enemy is refunded (see below). |
| SHIELD LOOK | BUILT-IN, MODEL | MODEL: the converted Digital Goblin shield, else the built-in rings and star. |

**BATON LOOK's default is CLASSIC** (the 0.0.62/0.0.63 baton). The player asked for the Yanez batons "as the default" baton
model. The standing rule, though, is that every new visual is off by default. So CLASSIC stays the
default, and MODEL is the first choice once the player opts in (CLUB if the files are missing).
Making MODEL the default is a one-value change, in the option's default and its test.

## What is real and what is a look

"Real" means a blow sent on sm2_damage's DirectDamage path (CONFIRMED, rep0050) or a write to the
CONFIRMED HeroHealth record. The damage amounts are the mod's own estimates.

| Feature | Real | Visual only / not built |
|---|---|---|
| Batarangs (one in each hand), CHAIN (max 3) and return | Each hit is a real blow | Flight and return are drawn |
| Grapple gun | Real movement: the bridge's ray finds the rooftop or ledge, the bridge's safe velocity path (FLY MODE's) pulls him up and over | The line and hook are drawn |
| Cape glide | Real movement on the same path: arms out glide, dip an arm to turn, tuck to dive, raise to pull up | — (no cape model) |
| Smoke | — | Visual only. It hides nothing (no CONFIRMED stealth function). |
| Heat vision | Real tick blows along the gaze, 3 s on and 2 s to cool | Beam |
| Cold breath | Real tick blows | Frost look. **Nobody is frozen** (no CONFIRMED freeze). |
| Super strength | Real: an extra blow on a landed punch | — |
| Superspeed | FLY MODE's SUPERSONIC dash (existing, real) | No ground speed (no CONFIRMED control) |
| Two pistols and the sawed-off | Real blows (pellets summed: one blow per enemy) | Muzzle flash and ammo readout |
| Reload, EASY RELOAD | Mod-side ammo state | Magazine and shells are drawn |
| Shell ejection | — | Visual, capped at shellsMost |
| Holsters (hips and back) | Grip arbitration, so a drawn grip never webs, zips or climbs, and haptic ticks | Items drawn on the body |
| Claws | Real blows (the 0.0.62 baton swing path) | Claws |
| Healing | Real: HeroHealth refill, never above the game's cap and never a revive | — |
| Shield throw (backhand, flat spin 8 rev/s, ricochet, return) | Each hit is a real throwHit blow (SHIELD DAMAGE) | Flight, spin and return are drawn |
| Shield bash (a swing with it on the arm) | Real: an 18 HP bashHit through the baton swing path | — |
| Shield worn / holstered (SHIELD ARM forearm straps, right by default; back) | The shared holster state machine: grip arbitration and haptic ticks | Drawn on the arm / back |
| Shield invincible | Never breaks, takes nothing (a mod-side object) | Paint wear is visual only |
| SHIELD BLOCK (rounds and hits on the shield) | **EXPERIMENTAL, CANDIDATE, OFF**: HeroHealth refund when the face points at the nearest enemy | The game does not say what hit you, so "contact" is a guess |
| Captain America suit | A game-side asset swap on the PC (docs/CAP-SUIT-PIPELINE.md) | Not loaded by the VR mod |
| Knockdown, disarm, real freeze, world slowdown | **Not built** (no CONFIRMED function), hidden | — |

## Game addresses

**No new address in 0.0.7.** Gate G7b checks that the diff adds no address literal. Everything
above uses paths that were already CONFIRMED (DirectDamage, HeroHealth, the zip launch, the
FLY MODE dash). HULK's leap and fling stay CANDIDATE, as in 0.0.62. No CANDIDATE address was added.
CAPTAIN AMERICA adds no address either: the shield's blows use DirectDamage and SHIELD BLOCK writes
the CONFIRMED HeroHealth record. Only SHIELD BLOCK's *rule* (that a hit "met the shield") is CANDIDATE,
and it runs only when the player picks EXPERIMENTAL.

## Models: format and paths

Models go in `<Spidy folder>\Models\`. Each one is a `scene.glb` (or `scene.gltf` + `scene.bin`) in:

```
Models\Batarang\EIRA\  Models\Batarang\CLOONEY\  Models\Batarang\JOKER\
Models\Pistol\  Models\SawedOff\Frame\  Models\SawedOff\Barrels\
Models\Baton\Left\  Models\Baton\Right\  Models\Shield\
```

- **Converted models** (from `tools/models007/convert_models.py <downloads> <out>`; needs `pip install numpy Pillow pyfqmr`, a dev tool only, not shipped):
  - metres, x right, y up, -z forward, with the grip at the origin;
  - a per-corner `COLOR_0`;
  - the sawed-off barrels carry `asset.extras.pivot` (the hinge);
  - 700 triangles at most per model.
- **JOKER** is copied byte for byte, at 396 triangles (CC BY-NC-ND).
- **The reader** (`mesh_model.hpp`) is strict. It refuses compression, out-of-range data, NaN, more
  than 200000 source triangles and files over 64 MB. A refused or missing slot draws the built-in
  look, with no crash and no flatscreen fallback. Missing and corrupt files are tested in core_tests.
- **Credits** are in `THIRD-PARTY-MODELS.md`.

### Triangles and textures (final)

| Model | Source triangles | In the mod | Source textures | Runtime textures |
|---|---|---|---|---|
| EIRA batarang | 13570 | 700 | 4 × 2048² | none (baked) |
| CLOONEY batarang | 1428 | 700 | none | none |
| JOKER batarang | 396 | 396 (unchanged) | 3 × 4096² | shipped unmodified, unused |
| 9mm pistol | 4707 | 699 | 6 × 2048² | none (baked) |
| Sawed-off | 38340 | 700 (frame 420 + barrels 280) | 9 × 2048² | none (baked) |
| Daredevil's baton | 5656 (rope 624) | LEFT 399 + RIGHT 400 | 512², 512², 128², 128², 256×128 | none (baked) |
| Captain America Shield | 22028 | 698 (disc 530 remeshed + straps 168) | from the download | none (baked, paint wear kept) |

## Renderer and budget

The overlay draws flat-lit triangles, sorted far to near. It has no depth buffer and no texturing.
A hero's models are appended to the frame's one vertex list, so they add no extra draw call.

| Hero | Worst case |
|---|---|
| Punisher | 2 × 699 + 700 = 2098 triangles (about 6.3k vertices) |
| Batman | 2 × 700 |
| Daredevil | 399 + 400 |
| Captain America | 698 (one shield: on the arm, on the back or in the air) |

The test asserts at most 2 weapon instances per hero in every state. It also checks the 700-triangle
cap on each model (the vertex budgets test). Sorting about 2k triangles each frame costs well under
0.1 ms on the CPU.

**Limit:** without a depth buffer, a model can draw over the world it is behind. This was already
true for 0.0.62's batons.

## Reload proposal (EASY RELOAD)

- **AUTO (default):** an empty gun refills after a short wait, with no gesture. This suits seated
  play and new players.
- **REALISTIC:**
  - a quick downward flick drops the magazine (the sawed-off breaks open and ejects its two shells);
  - touching the gun to your hip seats a new one, with a haptic tick and the mag_seat sound;
  - the readout shows "MAG OUT", then "RELOAD...".
- **Proposed next step (not built):** keep these two modes. If testers find the flick too easy to
  trigger while swinging, raise flickSpeed or require the gun to point down.

## Headset checklist

1. Start 0.0.62 settings unchanged and confirm nothing differs (all 0.0.7 options OFF).
2. **BATMAN ON.**
   - [ ] A batarang shows in each hand.
   - [ ] Throw with each trigger and see a hit, smoke, and a bounce at 25%.
   - [ ] It returns after at most 3 hits.
   - [ ] No web of any kind: RT, LT+RT, RB, LB+RB, Y at range and the right stick up never web; no web
         lines, balls, aim markers or web sounds. FLY MODE ON does not fly him.
   - [ ] Grapple: with no enemy in the sights, aim a hand above level at a rooftop or a ledge (within
         45 m) and pull the trigger: a whoosh, a dark line with a hook, a pull up to it, a pop over
         the edge and a tick on arrival. At nothing (sky, too far): a light buzz, no line.
   - [ ] Aimed level or down, or at an enemy: the trigger throws the batarang as before.
   - [ ] Cape glide: jump or step off a roof and hold both arms out to the sides: a buzz in both
         hands, a slow fall moving forward. Dip the left arm: turn left; the right: turn right.
         Tuck the arms in: dive, faster. Spread and raise them: pull up, then it settles back.
         Drop the arms or land: it stops. A grapple stops a glide.
   - [ ] Comfort: no snap turns or jolts at a start or an end of a glide or a grapple.
3. **BATARANG STYLE.**
   - [ ] EIRA, CLOONEY and JOKER each draw.
   - [ ] Renaming the Models folder draws DEFAULT, with no crash.
4. **SUPERMAN ON.**
   - [ ] Both triggers fire heat vision along the gaze and hurt enemies, cutting off after 3 s.
   - [ ] A hand at the mouth with both triggers gives cold breath (damage, frost look).
   - [ ] Punches hit harder.
   - [ ] FLY MODE's dash still works.
5. **PUNISHER ON.**
   - [ ] Both pistols and the sawed-off are visible on the body.
   - [ ] Each hand draws its own hip pistol. Try cross-draw with EITHER HAND.
   - [ ] Reach behind and grip to draw the sawed-off. There is a haptic tick on draw and on holster.
   - [ ] A drawn gun never webs or climbs.
   - [ ] Shells eject.
   - [ ] REALISTIC reload works: flick, then touch the hip.
   - [ ] GUN MODELS shows the models.
6. **WOLVERINE ON.**
   - [ ] Holding the trigger brings the claws out, and swinging them cuts.
   - [ ] Health climbs back after a few seconds without taking a hit.
7. **Daredevil.**
   - [ ] With BATON HOLSTERS ON, the batons sit on the back and draw over each shoulder.
   - [ ] BATON LOOK MODEL shows two batons with no cord.
   - [ ] A cord shows only on a throw.
   - [ ] BATON GRIP (BUILT-IN, CLUB and MODEL): each baton sits across the closed fist, the butt just
         under the heel of the hand (a third of it behind the hand), the tip leaving over the thumb and
         index knuckle about 20 degrees off the forearm (an escrima stick or a hammer), the same on both
         hands, mirrored. It no longer points straight out past the knuckles.
   - [ ] It stays put when AIM AXIS changes (HAND, AIM POSE, OLD): only throws follow the aim.
   - [ ] Swinging still hits where the baton is drawn; throws, the chain and catch, and drawing and
         putting away at the back are unchanged.
   - [ ] Valve Index (if you have one): the 15-degree handle tilt is unconfirmed (a CANDIDATE); note whether the batons
         sit too high or too low in the fist.
   - [ ] PUNISHER GUN GRIP (BUILT-IN and MODELS): each pistol's grip is in the palm, the web of the hand
         high on the backstrap, the index finger along the trigger guard, the barrel straight down the
         forearm and the sights level with the aim ray; the gun rolls with the wrist. Mirrored on both hands.
   - [ ] The sawed-off is held one-handed by its grip, barrels along the aim ray; it still breaks open on
         the hinge, sheds both cases and reloads. Shots leave the muzzle you see; pistol cases leave the
         ejection port to the right.
   - [ ] Valve Index: the guns use the same unconfirmed 15-degree (CANDIDATE) tilt; note if they point high or low.
8. **Performance.**
   - [ ] Watch frame timing with Punisher's three guns out. There should be no reprojection.
9. **CAPTAIN AMERICA ON** (SHIELD ARM RIGHT, SHIELD DAMAGE 35 HP, CHAIN ON, BLOCK OFF).
   - [ ] The shield sits flat on the back, star upright. Nothing is in the hands.
   - [ ] Reach over the RIGHT shoulder and grip: a tick, and it is on the right forearm, face out.
   - [ ] The LEFT hand cannot take it. Gripping it never webs, zips or climbs.
   - [ ] With the right hand on a web or a wall, gripping at the back does not draw it; the right
         grip in front still climbs as before (no web: only Spider-Man webs). The wrist HUD stays on the left wrist.
   - [ ] Flying (FLY MODE): the right trigger does not move you (it is the shield's).
   - [ ] Let go behind the shoulder: it goes back on the back with a tick.
   - [ ] A backhand flick plus the trigger throws it flat and spinning; it hits, ricochets to up to
         two more enemies and comes back to the arm (throw, clang, catch sounds).
   - [ ] SHIELD CHAIN OFF: one enemy, then home. SHIELD DAMAGE 70 HP: enemies drop faster.
   - [ ] A swing with it on the arm bashes (clang).
   - [ ] SHIELD ARM LEFT: it moves to the left arm, live, and only the left hand draws it (over the
         LEFT shoulder). Back to RIGHT: the right arm again.
   - [ ] SHIELD LOOK MODEL: the converted shield. Rename `Models\Shield`: BUILT-IN, no crash.
   - [ ] SHIELD BLOCK EXPERIMENTAL (optional): face an enemy with the guard up and take a hit; health
         should come back. Note whether it also refunds hits from behind (the CANDIDATE guess).
   - [ ] No CAP LOOK overlay anywhere.
10. **Captain America suit** (optional, PC): follow docs/CAP-SUIT-PIPELINE.md and check it in game.

## Previews (no headset)

`apps/hero007_preview.cpp` and `tools/models007/render_models.py` render the preview PNGs (into a folder
of your choice). CAPTAIN AMERICA's previews (`cap/`) come from `tools/models007/render_cap.py`
and `tools/models007/cap_flight_diagram.py` (a box-side tool; `pip install matplotlib`). The package
check allows matplotlib only under `tools/models007/` and Blender's `bpy` / `mathutils` / `bmesh` only under
`tools/cap_suit/blender/` (those scripts run inside Blender on the PC).

## Changelog

- **CAPTAIN AMERICA** (hero 8, combat38 options 65-70, all OFF / 0.0.62 by default):
  - The shield (`include/spidy/captain.hpp`, `hero007_play.hpp`): worn on the SHIELD ARM's forearm
    (straps and handle, default RIGHT, LEFT selectable), holstered flat on the back through the shared holster state
    machine (grip over the shoulder, haptic tick, never a web-zip or climb clash), drawn only by its arm's
    hand. Thrown backhand like a frisbee (the trigger; a fast forward sweep sets the direction), it flies
    flat spinning 8 rev/s, ricochets like the batons (SHIELD CHAIN, 3 enemies at most) and returns.
    Hits are real blows on sm2_damage's CONFIRMED path (SHIELD DAMAGE 35/20/50/70 HP); a swing on the
    arm bashes (18 HP). The shield is invincible (never breaks). SHIELD BLOCK EXPERIMENTAL (CANDIDATE,
    OFF) refunds a hit while the face points at the nearest enemy, never above the cap, never a revive,
    never with INVINCIBLE on.
  - Looks: BUILT-IN rings and star, or SHIELD LOOK MODEL (`Models\Shield`, Digital Goblin, CC BY 4.0,
    22028 -> 698 triangles, paint wear visual only). Four new sounds (shield_throw, shield_ricochet,
    shield_catch, shield_clang; 20 in all). No CAP LOOK overlay.
  - The wrist HUD's POWERS page has a CAPTAIN AMERICA section (10 sections); the live settings,
    exports (5387-5392) and update records carry the six options.
  - The suit: prepared on the box (`CapSuit/` in the release zip, `tools/cap_suit/`), swapped on the PC
    with Luna Engine IO + Overstrike (docs/CAP-SUIT-PIPELINE.md).
  - Research applied: the 0.61 m (24 in) disc on the forearm through two straps; the backhand
    frisbee throw; a gyroscopically stable flat flight at about 25 m/s and 8-10 rev/s; near-elastic
    ricochets; the return to the hand.
  - Gates: G7b checks the five CAPTAIN AMERICA tests (incl. the RIGHT-arm clash test), the defaults, both credits and no CAP LOOK; new
    **G1b** (pyflakes: no undefined name, nothing new against e077d35) and **G9b** (the launch path,
    `tests/session_launch_tests.py`).

- **PUNISHER GUN GRIP** (the user: "make sure punishers gun sit right in his hand"). Cause: a held gun was
  drawn along the hand's AIM (AIM AXIS HAND: the grip's -Y, 30 degrees below the forearm on Touch), kept
  upright to the world (`gunUp`, so it never rolled with the wrist), with the look's origin (the front
  of the grip, at the trigger guard) on the palm, 2.3 to 5.7 cm off the grip. Fix
  (`include/spidy/gun_grip.hpp`, `include/spidy/hero007_play.hpp`, `src/hero007_visual.cpp`,
  `src/game_xr.cpp`): from the controller's OpenXR grip pose, the barrel runs along the forearm (the
  grip's +Y tilted by the handle: Touch 30 degrees CONFIRMED in this project, Index 15 unconfirmed (CANDIDATE), others
  as Touch), which on Touch is the runtime's aim ray; the gun's top is toward the thumb, so it rolls with
  the wrist and mirrors left and right. Each look's grip middle (`palmPoint`: pistol model -0.026/-0.034 m,
  built-in -0.028/-0.012; sawed-off model -0.038/-0.016, built-in -0.018/-0.015, forward/up) sits on the
  palm. Shots leave the drawn muzzle along the barrel (`muzzleFromPalm`), so the sights are on the shot;
  pistol cases leave the ejection port (`breechFromPalm`), the sawed-off's two from its breech when it
  opens. With AIM AXIS HAND the guns now fire along the barrel (the forearm) instead of the grip's -Y.
  The hinge and break-open, reload, holsters, the hip and flick, and one gun per hand are unchanged.
  With no usable grip pose, 0.0.62's placement and shot are kept. No new setting, no game address.
  Test: "Version 0.0.7: PUNISHER GUN GRIP - in the palm from the grip pose: barrel on the forearm and the
  aim ray, mirrored, per controller" (in G7b, with a source check). Previews:
  `tools/models007/render_guns_grip.py` (before and after, first person and side, both looks, open too).
  Sources: as for BATON GRIP below.
- **DAREDEVIL BATON GRIP** (the user, public 0.0.63: "daredevils batons sit weird in his hands not like a
  natural grop"). Cause: the held baton was drawn along the hand's AIM (`Frame::Hand::aim`: AIM AXIS HAND
  is the grip's -Y, webShooter's pointing line; AIM POSE is the runtime's aim pose), from 0.12 m behind the
  grip point to 0.45 m ahead. On Touch that is 30 degrees below the forearm toward the little finger, a
  pointer sticking out past the knuckles, its butt inside the wrist and the hand only 21% of the way
  up; it also turned with AIM AXIS and the runtime. It was never placed from the grip pose. Fix
  (`include/spidy/baton_grip.hpp`, `src/game_xr.cpp`): the held baton now comes from the controller's
  OpenXR grip pose only. It runs through the fist in the grip's Y-Z plane, 20 degrees off the forearm
  toward the thumb (`offForearmDeg`, the test allows 15-25), with the hand a third of the way from the
  butt (`gripShare`) on a 0.57 m shaft. The forearm is the grip's +Y tilted by the handle,
  per controller (`runtime.controllers()`): Touch 30 degrees (CONFIRMED in this project, tracking.hpp
  Update 20), Index 15 degrees (unconfirmed: CANDIDATE, headset check), Steam Frame and others as Touch (CANDIDATE).
  Left and right mirror by construction (+X is to the right on both hands). The in-hand swing's
  shaft uses the same line, so hits land where the baton is drawn. Throws, the chain, the aim assist, the
  holster draw and put-away and the one-per-hand rule still use the hand's position and aim, unchanged.
  With no usable grip pose (NaN, not unit), 0.0.62's shaft is kept. No new setting, no game address.
  Test: "Version 0.0.7: DAREDEVIL BATON GRIP - in the fist from the grip pose: 15-25 deg off the forearm,
  hand at 1/3, mirrored, per controller" (in G7b, with a source check that game_xr draws from the grip
  pose). Previews: `tools/models007/render_batons_grip.py`:
  first-person left and right and a side view, before and after, for the built-in baton and the Yanez
  model. Sources: the OpenXR 1.1 specification, "Standard pose identifiers" (grip: the pose for rendering
  a held object; +X palm normal, -Z through the curled fingers toward the thumb); KhronosGroup/OpenXR-Docs
  issue #118 (the grip's tilt differs per controller); Unity forum OpenXR threads (Touch aim vs grip
  about 45 degrees; Index aim and grip as specified).

- **SHIELD ARM RIGHT by default** (the user, 26 Oct: "it should be on the right arm... or toggable
  between arms"): the values are now RIGHT, LEFT (RIGHT first, the default; LEFT live-selectable).
  Stored value 0 now means RIGHT (it meant LEFT in an earlier test build; nothing was released).
  Checked against everything on the right hand: grip-to-draw only at the back holster and never while
  that hand is on a web or a wall (web-zip, climb untouched), the right grip anywhere else is never
  claimed, the left hand is left alone, Punisher's and Daredevil's one-per-hand racks are unchanged
  (one hero at a time), and the wrist HUD stays on the left wrist. New test: "CAPTAIN AMERICA SHIELD
  ARM RIGHT (default): no clash". Previews: `cap/cap_shield_right_forearm.png` (the default, new),
  `cap_shield_left_forearm.png` (the LEFT setting), the back holster and throw re-rendered on the right. In flight the game's combat buttons are zeroed for every hero but
  Iron Man (unchanged), so the shield is not thrown while flying.

- Launch fix: `tools/run_game_vr.py` `session()` read `session_off` (the settings a session turns off
  while VR runs, from `live.txt` `lens_flares = off_in_vr`) from `main()`'s locals, so every real launch
  stopped with `NameError: name 'session_off' is not defined` at stage "starting the game". `main()` now
  passes it: `session(a, startup, session_off=session_off)`; the default `()` keeps the game's own
  settings. The same bug is in 0.0.62 (e077d35), which this branch does not change.
  `tests/session_launch_tests.py` runs `main()` -> `session()` -> `prepare_display()` with the game launch
  mocked (any OS), so this kind of error fails a gate instead of a launch.
- FLY MODE trigger fix (the user, 26 Oct: "if youre iron man and you shoot your hand palm you actually fly
  around"): `src/game_xr.cpp` packed each hand's raw trigger into FLY MODE's thrust for every hero, so a
  trigger that fires a weapon also flew the player. Now `hero_modes::flyThrust` sends 0 thrust for the
  trigger-weapon heroes (`hero_modes::triggerWeapon`: IRON MAN, DAREDEVIL, BATMAN, WOLVERINE, PUNISHER);
  they fly by the poses (a hand up rises, both hands forward fly, a T-pose stops), the left stick, the
  right stick's rise and R3, unchanged. SPIDER-MAN, SUPERMAN and HULK keep the trigger thrust. No new
  setting, no game address. Test: "Version 0.0.7: FLY MODE - a trigger-weapon hero's trigger never moves him".
  Merged locally into hero-0.0.7 (fly-trigger-fix 754ef9d unchanged) and extended to CAPTAIN AMERICA: his
  shield throw is the SHIELD ARM hand's trigger (RIGHT by default), so `triggerWeapon` lists him too and
  neither trigger thrusts him in FLY MODE (poses and sticks still fly him). Test: "Version 0.0.7: FLY MODE -
  CAPTAIN AMERICA's shield throw never flies him (either SHIELD ARM)".

## BATMAN grapple gun and cape glide; webs are Spider-Man's only

The user (27 Oct): Batman has no flying and no webs; he gets a grapple gun and an Arkham Shadow
style cape glide, and every other hero's web abilities are off.

- `include/spidy/cape_glide.hpp` (pure): `Glide` reads both hands against the head in the body's
  frame. Arms out to the sides (more than 1.0 m apart, between shoulder-low and head height) for
  0.1 s while in the air for 0.12 s starts the glide (heading from the body's motion, or the look).
  Energy model: speed changes by -g sin(pitch) - drag * speed^2; a glide (-10 degrees) settles near
  14 m/s, sinking about 2.4 m/s; a tuck dives (-50 degrees, toward 30 m/s); raised arms pull up
  (+25 degrees) while there is speed, then stall back to a sink. The arms' tilt turns him (6 degrees
  dead zone, full turn 85 degrees a second at 35 degrees). Landing, a wall, the arms dropped, lost
  hands or being blocked for 0.45 s end it.
- `Grapple`: a hit within 2.5-45 m on a fixed surface (never the air, never a moving body). A
  rooftop (normal up): pulled to stand on it; a wall face: to 1.6 m above the hit, 0.6 m out, then
  a 0.32 s pop up and over the edge. Up to 24 m/s, slowing near the end; no progress for 0.5 s or
  3 s in all lets go.
- VR side: `hero007::Play::stepBatman` takes a trigger press as the grapple's when no enemy is in
  the batarang's sights and the hand points at least 10 degrees above level (`grappleAim`); that
  press throws no batarang. `game_xr.cpp` sends it as `Command::asks` bits 6/7 with bit 5 (BATMAN);
  FLY MODE is off for him. The bridge (`game_swing.cpp`) fires the ray from the hand's aim, runs the
  grapple and the glide next to FLY MODE on the same capped velocity path, and publishes
  `SpidyCapeSample` (optional: an older bridge draws no line). The line: `hero007::appendGrappleLine`.
  Sounds: the existing baton whoosh (fire) and holster tick (arrival); never a web sound.
- Every hero but Spider-Man: the hands' triggers and grips no longer reach the bridge's webs, web
  balls or web grabs (climbing is unchanged), the aim markers are Spider-Man's, and the combat
  config has no web yank and no game Y web strike (a punch at range uses the bridge's own lunge,
  no web line). No new setting; BATMAN ABILITIES' subtext says it. Live, no restart.
- Tests: "Version 0.0.7: BATMAN cape glide (...)", "BATMAN grapple gun (...)", "BATMAN trigger -
  grapple aimed up with no enemy, batarang otherwise", and the grapple line's budget.
