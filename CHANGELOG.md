# HERO VR changelog

HERO VR - Made by Agent Mulder. Full history (back to Spidy): [docs/CHANGELOG.md](docs/CHANGELOG.md).

## v0.0.70 - HERO VR 0.0.7 (October 26, 2026)

Version 0.0.70 (build Update 39.4), launcher build 11 (0.0.70.1). "0.0.7" is published as 0.0.70 because versions
compare number by number (7 < 63); the trailing 0 keeps it above 0.0.63 for every launcher (docs/VERSIONING.md).
No new game address: every blow uses the CONFIRMED damage path, healing and SHIELD BLOCK the CONFIRMED HeroHealth record.

- New heroes, all OFF by default (one hero at a time): BATMAN (batarangs with BATARANG CHAIN, grapple, smoke as a look
  only), SUPERMAN (heat vision, COLD BREATH - real tick blows, nobody is frozen -, super strength, the SUPERSONIC
  dash), PUNISHER (two pistols and a sawed-off, holsters, EASY RELOAD), WOLVERINE (claws, healing) and CAPTAIN AMERICA
  (shield worn, drawn, thrown with SHIELD CHAIN, bash; SHIELD BLOCK is an OFF-by-default experiment).
- Guns fire along the barrel: pistols and sawed-off sit in the palm from the OpenXR grip pose and shots leave the drawn
  muzzle. Daredevil's batons sit in the fist. Valve Index 15-degree tilt: unconfirmed (CANDIDATE).
- Optional models (BATON LOOK, BATARANG STYLE, GUN MODELS, SHIELD LOOK) from `Models\` in the release zip, with a
  built-in fallback. Credits: THIRD-PARTY-MODELS.md and CREDITS.md. The JOKER batarang (CC BY-NC-ND 4.0) ships unmodified
  with its licence.
- MIRROR VIEW: kept across the restart the wrist asks for, and the test turns itself OFF (saved) after its 30 s run.
- Captain America suit: a manual guide (CapSuit/, docs/CAP-SUIT-PIPELINE.md); nothing installs it.
- Launcher build 11: smoother opt-in HERO VR design (60 fps menu, seamless orb loop at the concept speed); classic stays
  the default with its fallback. Revert offers 0.0.63, 0.0.62, 0.0.6 and 0.0.5.
- Smoother cutscene starts: the mod no longer asks the game for two hidden 3D eye views (about 1-1.25 s at every
  cutscene start or resume) while the HUD panel's hook, without which they cannot be shown, is not installed (SM2:
  never yet). Cutscenes stay on the flat screen as before; GAME SCREEN 3D's text says so.
- Fixed: the launch settings' `session_off` now reaches prepare_launch on the real launch path.
- Release cleanup: test logs untracked, debug notes go to the log only, build paths removed from shipped files.

## v0.0.63 (October 26, 2026)

Version 0.0.63 (build Update 39.3), launcher build 10 (0.0.63.1). The mod is 0.0.62's plus one fix.

- Fixed: 0.0.62 failing to start VR. PLAY VR stopped at "starting the game" with `NameError: name 'session_off' is
  not defined` (run_game_vr.py read a variable of main() inside session()). The lens-flare choice now travels with
  the launch settings. New gate: tests/launch_session_tests.py runs the real launch path (main -> session ->
  prepare_display) with stand-in Windows modules, plus a pyflakes check for undefined names in tools/*.py.
- New opt-in launcher design (HERO VR orb, soundscape, big menu). Classic stays the default; the launcher asks once.
  Options > Launcher switches either way; Shift or --classic opens classic once; if the new design cannot start, the
  classic one opens and says why, once.
- Revert offers 0.0.62, 0.0.6 and 0.0.5. Updating from any earlier launcher installs 0.0.63 exactly once.

## v0.0.62 (October 26, 2026)

Version 0.0.62 (build Update 39.2), launcher build 9 (0.0.62.1). Built on 0.0.6 and its hotfix. No new game address:
every hit uses the CONFIRMED damage path, every wall test the CONFIRMED ray bridge. Graphics defaults unchanged.

- Ghost hands: EYE AA no longer keeps history far outside a pixel's neighbourhood, so fast hands leave no copies.
- BOLT WALLS: repulsor bolts, the chest beam and web balls stop at the first wall (fail safe: no answer, no wall).
- LENS FLARES (GRAPHICS, takes effect at the next VR start): OFF IN VR turns off the game's flares (headlights and
  bridge lights through walls at night); GAME, the default, leaves them.
- Combat: PUNCH HITS (GAME + INSTANT by default) lands a small hit when the fist touches; ENEMY TRACKING (LIVE by
  default) follows enemies at 30 Hz by their own positions. 0.0.6 behaviour one setting away.
- Daredevil: batons hit in the hand and on straight throws; a thrown baton bounces between up to 3 enemies (next within
  5 m, never the same twice, skipping downed enemies) and always returns. BEACONS IN VISION (ON): beacons and markers
  stay bright and coloured in the sonar.
- Iron Man: a new CHEST BEAM sound made from the player's repulsor sound, and the bolts and the chest beam shimmer like plasma.
- Iron Man: the hand blast uses a player's own repulsor sound (thank you!), fires from the palm along its normal, and
  hand repulsor blasts have a 2-second cooldown per hand (a ring on each wrist). The CHEST BEAM does 4x a blast's damage
  with its 30-second cooldown shown on the wrist ring, as before.
- REAL ANCHORS (launcher, ON): a swing starts only when the web meets a surface within 100 m; otherwise a miss and a
  buzz. OFF keeps the air swing. HIT INDICATOR (OFF) and HAND FLIPS (ON = 0.0.6).
- Launcher build 9: Revert offers 0.0.6 and 0.0.5; numeric version compare; first-run fixes for bracket folders,
  running inside the zip and antivirus removing the launcher; crash log falls back to %TEMP%.

## v0.0.6 hotfix (October 26, 2026)

Version 0.0.6 (build Update 39.1.1), launcher build 8 (0.0.6.6; build 7 = 0.0.6.5 was the first hotfix launcher).

- Launcher build 8: fixes "No working Python 3.11 or newer was found" after updating to the hotfix. The update from
  launcher build 6 kept the old mod files for the same version 0.0.6 and, doing so, left out the mod's own Python
  (python folder). The launcher now checks python\python.exe and the mod files' build after every install, update and
  start, and before PLAY VR, and downloads the ready-made mod files again by itself when anything is missing or older.
  Players never install Python. The release mod zips (0.0.5 and 0.0.6) always had their Python; a new gate checks it.
- The update from launcher build 6 also now replaces the Update 39.1 mod files with the 39.1.1 ones (the swapchain fix).
- A blank or placeholder update-repository setting falls back to lecutis7of9/HEROVR.

- VR no longer falls back to flat screen when the headset runtime refuses the eye images at start ("Create eye
  swapchain XrResult=-2", seen with Virtual Desktop). The mod tries again with the 0.0.5 settings (the same request,
  after 0.5 s and 1.5 s), then without the transfer-source usage, then another colour format, then smaller eye sizes
  (90/80/70/60%), and writes every try to the console. If all fail it keeps retrying the session every 2 s (5 s after
  2 minutes, with advice to restart Virtual Desktop and the game). No game addresses changed.

## v0.0.6 (October 25, 2026)

Version 0.0.6 (build Update 39.1), launcher build 6. Built on 0.0.5 (with its hotfixes 2 and 3).

- Hero powers with real damage: IRON MAN (repulsors, the chest beam aimed at the enemy you look at), DAREDEVIL
  (batons that chain to up to 3 more enemies, DAREDEVIL PUNCH, sonar) and HULK (charged jump, knockouts). One hero at a
  time, all OFF by default, all live.
- New Iron Man repulsor look and sound (no new render pass).
- The hologram in 8 pages with named sections; RESTART tags on the settings that need one.
- Opt-in reports (crash report, Report a bug, play stats off by default); nothing is sent by itself.
- Frame rate (Update 39.1): measured 34.5-37 new frames a second at 90 Hz, GPU-bound (the headset copy waits about
  6.6 ms for 0.19 ms of work; Spidy's own work is under about 1 ms). Session advice for 72/80 Hz and RenderScale
  90/80 (+14% / +30%, estimates from the RTX 4080 measurement); AUTO PERFORMANCE's AUTO target is half rate when full
  rate is out of reach; stalls are counted only during play. Graphics defaults unchanged. No new game address.
- VR-loss fix (Update 39.0.3): a lost OpenXR session is recovered (the mod retries every 2 s) instead of leaving the
  game flat.
- Launcher build 6: Revert > USE PREVIOUS VERSION (0.0.5) downloads and SHA-256 checks the v0.0.5 release, installs
  it and skips 0.0.6 until Updates > Update now.
- `tools\build.ps1` run by a launcher installs the release's ready-made mod files (never compiles); `-FromSource` builds.

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
