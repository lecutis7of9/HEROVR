# HERO VR 0.0.76 - Spider-Man melee snap and sure web balls (SM2)

From test 801ce57 (the owner): "hits are still hard to land as spiderman and webballs only fire out like a quarter of
the time and even then only in combat".

## MELEE SNAP (include/spidy/melee076.hpp, combat_input.hpp; on by default, `Config::meleeSnap`)

Why punches whiffed in that run:
- most combat swings were held back as "too slow" because the lunge owned them (a lock on an enemy past LUNGE FROM),
  and some as "not forward" / "not extended", measured toward the lock instead of the enemy he swung at;
- the lunge's aim turned punches at a thug 1.2 - 2.7 m away into the game's Y web strike.

Now each swing picks **the enemy it is thrown at**: among the enemies in sight within 3.5 m (flat on the ground, 3D in
the air), the one inside 70 degrees of the fist's way (smaller angle, then nearer, first); with none there, the
nearest one within 1.8 m in front. A swing with a snap target:
- is measured toward **him** (the usual PUNCH SENSITIVITY speed and reach, so a walking arm swing is still no punch);
- is never held back by the lunge, never the zip strike;
- sends the game's own X (its combo) and the **PUNCH HITS blow (the CONFIRMED damage path) to him**, and locks him.

Any enemy in sight within 3.5 m keeps the lunge (the game's Y) shut, so close fights are all punches. One long sweep
at him is one blow (no re-arm by time alone, no extra contact). Enemies behind a wall are never snapped.
Log: `VERSION 0.0.76 MELEE SNAP: <n> punches sent at the enemy swung at (<frames> frames the lunge stayed shut...)`.
Tests: tests/melee076_tests.cpp, tests/core_tests.cpp "HERO VR 0.0.76 MELEE SNAP", the replay suite (no arm-swing X).

## BALL SURE (include/spidy/ball_sure.hpp, game_xr)

The game fires its R1 web ball only in combat, and not on every tap. No CONFIRMED SM2 function fires the game's ball,
so none is called. Per press, for Spider-Man:
- **out of combat** (no enemy within 30 m): no RB is sent; the mod's own ball leaves that wrist at once;
- **in combat**: the RB goes to the game (its ball, look and auto-aim). The wrist muzzle hook (CANDIDATE) counts the
  web shooter's muzzle asks; hooked and no ask within 0.3 s, the mod's own ball leaves that wrist instead. Unhooked
  (nothing to tell), the mod's ball goes with every press, so a press is never lost (at worst two balls).
Log: `VERSION 0.0.76 BALL SURE: <presses> presses to the game, <n> its own shots seen, <n> backups, ...`.
Tests: tests/melee076_tests.cpp `ballTests`.
