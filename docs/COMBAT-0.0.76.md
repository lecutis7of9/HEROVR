# HERO VR 0.0.76 - Spider-Man melee snap and sure web balls (SM2)

From test 801ce57 (the owner): "hits are still hard to land as spiderman and webballs only fire out like a quarter of
the time and even then only in combat".

## MELEE SNAP (include/spidy/melee076.hpp, combat_input.hpp; on by default, `Config::meleeSnap`)

Why punches whiffed in that run:
- most combat swings were held back as "too slow" because the lunge owned them (a lock on an enemy past LUNGE FROM),
  and some as "not forward" / "not extended", measured toward the lock instead of the enemy he swung at;
- the lunge's aim turned punches at a thug 1.2 - 2.7 m away into the game's Y web strike.

Now each swing picks **the enemy it is thrown at**: among the enemies in sight within 2 m (3.5 m in 32bd648) (flat on the ground, 3D in
the air), the one inside 70 degrees of the fist's way (smaller angle, then nearer, first); with none there, the
nearest one within 1.8 m in front. A swing with a snap target:
- is measured toward **him** (the usual PUNCH SENSITIVITY speed and reach, so a walking arm swing is still no punch);
- is never held back by the lunge, never the zip strike;
- sends the game's own X (its combo) and the **PUNCH HITS blow (the CONFIRMED damage path) to him**, and locks him.

Any enemy in sight within 2 m keeps the lunge (the game's Y) shut, so close fights are all punches. One long sweep
at him is one blow (no re-arm by time alone, no extra contact). Enemies behind a wall are never snapped.
Log: `VERSION 0.0.76 MELEE SNAP: <n> punches sent at the enemy swung at (<frames> frames the lunge stayed shut...)`.
Tests: tests/melee076_tests.cpp, tests/core_tests.cpp "HERO VR 0.0.76 MELEE SNAP", the replay suite (no arm-swing X).

## PUNCH DASH (test 32bd648 feedback; melee076::dash, combat_input.hpp)

The owner on 32bd648: "hits land more but now you have to run to them, before if they were far and you hit in their
direction you would kinda dash right to them with the punch". Why: in 32bd648 any enemy within 3.5 m shut the lunge,
and the 3.5 m snap turned far swings into an X in the air (or "too slow" while the lunge owned them).

Now:
- the snap (X at him) is for enemies **within 2 m** only; with one that close the lunge stays shut (never a dash then);
- **2 - 15 m along the fist** (inside 35 degrees of the punch's way, flat on the ground, 3D in the air, in sight): the
  punch itself is the game's web strike (Y) at **that** enemy - the stick is steered at him (STRIKE STEER) and he is
  locked. The smaller angle wins, then the nearer. One dash at a time (0.6 s);
- with no enemy along the fist, the lunge of 32bd648 and before (the look and the fist together) still takes far
  swings, so throws at an enemy 4 - 18 m zip as before (replay: 180 of 181 first throws).
Log: `VERSION 0.0.76 MELEE SNAP: <n> punches sent at the enemy swung at (within 2 m), <n> PUNCH DASH web strikes at one
2 - 15 m away`. Tests: tests/melee076_tests.cpp `dashTests`, tests/core_tests.cpp "HERO VR 0.0.76 MELEE SNAP" and
"Update 38 SpidyVR: ZIP MODE" (a punch thrown at him while looking away dashes). The old lock-lunge tests run with
`meleeSnap = false` (Update 38's rule).

## BALL SURE (include/spidy/ball_sure.hpp, game_xr)

The game fires its R1 web ball only in combat, and not on every tap. No CONFIRMED SM2 function fires the game's ball,
so none is called. Per press, for Spider-Man:
- **out of combat** (no enemy within 30 m): no RB is sent; the mod's own ball leaves that wrist at once;
- **in combat**: the RB goes to the game (its ball, look and auto-aim). The wrist muzzle hook (CANDIDATE) counts the
  web shooter's muzzle asks; hooked and no ask within 0.3 s, the mod's own ball leaves that wrist instead. Unhooked
  (nothing to tell), the mod's ball goes with every press, so a press is never lost (at worst two balls).
Log: `VERSION 0.0.76 BALL SURE: <presses> presses to the game, <n> its own shots seen, <n> backups, ...`.
Tests: tests/melee076_tests.cpp `ballTests`.

## BALL NOW (test 32bd648 feedback; ball_sure::oursEveryPress, combat_input.hpp, game_xr)

The owner on 32bd648: "webballs are still not super reactive and dont fire unless in combat". Why (from the code):
- out of combat 32bd648 already fired the mod's own ball, but its draw loop skipped every mod ball under GAME WEBS ONLY
  (the Spider-Man default), so those balls flew **invisible** (and so did the in-combat BALL SURE backups);
- out of combat the ball also waited out the Sonic Burst's two-hand grace (about 0.17 s) and then fired at the let-go;
- in combat the press waited on the burst grace, the gadget waits, the RB queue and the game's own gate, plus up to
  0.3 s for the BALL SURE backup.

Now, for Spider-Man under GAME WEBS ONLY, **every press is the mod's ball, from that wrist, on the press frame**, in
and out of combat: no RB goes to the game, so the game never adds a second ball. The balls and their webbed-up cocoons
are drawn under GAME WEBS ONLY. A hit sends the small web-ball blow (10, no knockback) through the CONFIRMED damage
path. The +GAME WEB on hit stays on its BALL HIT setting (OURS ONLY by default). Gestures (gadgets, Sonic Burst) still
read the press as before. Tests: tests/melee076_tests.cpp `ballTests` (oursEveryPress), tests/core_tests.cpp
"HERO VR 0.0.76 BALL NOW" (in and out of combat: one ball on the press frame, no RB).
