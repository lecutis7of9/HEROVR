# HERO VR changelog

HERO VR - Made by Agent Mulder. Full history (back to Spidy): [docs/CHANGELOG.md](docs/CHANGELOG.md).

## v0.0.5 (October 25, 2026)

- Launcher build 4 (hotfix, mod files unchanged): VR lost (e.g. back to the desktop in Virtual Desktop) or a headset
  that is not connected no longer leaves the game silently on its flat screen. PLAY VR says "Headset not connected:
  open Virtual Desktop in your headset first" and waits for it (up to 5 minutes) without starting the game; a
  Spider-Man 2 left running is offered to be closed first; when VR is lost mid-game the launcher offers to close the
  game and start VR again (VR cannot restart inside the same game process in 0.0.5).
- First public HERO VR release, with the HERO VR launcher (new background, "Made by Agent Mulder").

**Version 0.0.5 (build Update 38.9, October 25).** Built on 0.0.4. The player, after the 0.0.4 headset test: punches
still miss ("it needs to register every single hit"), the damage path should be "the same logic as a punch", a hero
height for a big skin, and a Daredevil baton that chains. Then: the beam's cooldown on the wrist, more forgiving
punches, a faster FLY with a SUPERSONIC dash, and sound effects. Every new experimental setting starts OFF; none
needs a game restart. The zips are sm2-vr-0.0.5.zip and sm2-vr-0.0.5-slim.zip (under 25 MB, no docs or images).

- PUNCH SENSITIVITY: FORGIVING (default) / MAX / 0.0.4 / STRICT (new, live). FORGIVING takes slower (0.78x of 0.0.4's
  punch speed), sloppier (0.7x the forward share, 0.75x the reach) and quicker re-punched jabs (re-armed after a
  0.4 m/s pull-back). Replay P5: rep0030 177 blows (0.0.4 151) of 169 forward fists over 1.2 m/s, rep0023 175
  (0.0.4 155) of 174, each its own X, mean delay 7-9 ms; 9/9 synthetic punches land (0.0.4 8/9: a 5-jab flurry with
  8 cm pull-backs was 1 blow, now 5); 14/14 everyday motions (waving, a hand dropped, scratching the head ...) send
  no X; run38/run381 waving with no enemy: X 0. MAX takes any forward hand (shown only, not sent, with no enemy).
- DAMAGE PATH (the same logic as a punch). Read from Spider-Man2.exe: the game's melee site 0x8558c7 builds the
  victim's handle from the actor table slot and queues a request with the DamageSystem through DirectDamage
  (RVA 0x3412ec0), which takes a pool slot from 0x3412da0 (lock xadd). STATIC-STRONG, CANDIDATE live.
  - DAMAGE PROBE (read-only, always on): the nearest enemies' hit points (BotHealth, found the way the CONFIRMED
    HeroHealth is) at 10 Hz, every fall put beside the last X press. The log says "VERSION 0.0.5 DAMAGE PROBE":
    CONFIRMED once an enemy's HP falls within 1 s of an X.
  - DAMAGE TEST: OFF (default) / ON (EXPERIMENTAL, live). ON sends one DirectDamage request per web ball or repulsor
    hit, the melee site's own way, after checking DirectDamage's bytes, the system's vtable and that the victim is a
    live actor slot; every touch is SEH-guarded and a fault turns it off (no crash). CANDIDATE until the probe sees
    an HP fall after the mod's own request with no X press (write state CONFIRMED).
  - Until then Iron Man's damage, the DAREDEVIL / HULK switches and the baton chain in the game stay unbuilt (no
    do-nothing switches); they are built as soon as the headset log says CONFIRMED.
- HERO HEIGHT: 1.00X (default) .. 1.60X (new, live). The VR body (not the view) is k times the player, so a big hero
  skin (Hulk) stands straight instead of crouching. 1.00X changes nothing.
- DAREDEVIL BATON CHAIN (logic, tests and preview; in the game once damage is CONFIRMED): a hit chains to the nearest
  other enemy within 6 m of the one hit, never one already hit on that throw, at most 3 chains (4 enemies), then
  returns; a miss or no enemy in range returns at once.
- BEAM RING: IRON MAN's chest beam cooldown is now a ring on the right wrist that fills clockwise over the 30 s,
  then flashes with a chime and a buzz when ready; the floating "BEAM n S" line is gone. Preview
  images/beam-ring-005.png.
- FLY MODE: FAST is now 40 m/s (was 28). SUPERSONIC: ON (default) / OFF and SUPERSONIC SPEED 2X (default) / 1.5X /
  2.5X / 3X of FAST (80 / 60 / 100 / 120 m/s), both new and live. Flying with both hands out (the FORWARD pose), push
  the left stick forward past 60% to dash; let go (under 40%) or drop the pose and you are back to FLY exactly as
  before. When the dash first kicks in (and again only after 1 s off): a sonic boom sound and a 0.9 s shockwave, a
  ring that rushes past you and a vapour cone flaring back. Nothing else in FLY changes: SLOW, NORMAL and the poses,
  turn, roll, rise, landing, the vignette and INVINCIBLE are as in 0.0.4. Preview images/supersonic-005.png.
  Honest: the game moves the hero at most 2 m a game step, so the top speeds need a high frame rate (at 60 fps the
  dash tops out near 105 m/s, so 3X reaches about 2.5X; at 72 fps and up all four differ). The flight speed is in the
  log ("flying n m/s now") and is CANDIDATE until a headset run; very fast flight may outrun the game's world
  streaming (low-detail buildings for a moment).
- SOUND EFFECTS: the mod's own original sounds (procedural, tools/sfx/make_sfx.py; assets/sfx): repulsor blast,
  chest beam, beam ready and sonic boom (a baton whoosh and hit are built in for the batons). They play on their own XAudio2 thread (loaded at run time;
  if audio fails: silence and a code in the log, the game and VR untouched). SFX VOLUME 100% (default) / 75% / 50% /
  25% / OFF (new, live). The game's own audio engine is not used (a hook stays CANDIDATE).
- PACKAGING: build.ps1 runs bootstrap.ps1 by itself when third_party is missing and names what is missing; the
  release gate unzips the slim zip into an empty folder, adds only what bootstrap fetches, checks every Python import
  and every file CMake names (tools/package_check.py), then builds and runs the tests there.
- Kept: no flicker, the left-wrist health ring, no kicks from waving, zip kick, grab assist, yank, WEB ROPE, graphics
  defaults, FLY MODE, INVINCIBLE, NOMINMAX project-wide and the windows header gate. Save and story flags untouched.
  Tests: 589 unit tests, tests/v005_tests.py, replay P5. "VERSION 0.0.5" on STATUS, in the log and the reports.
