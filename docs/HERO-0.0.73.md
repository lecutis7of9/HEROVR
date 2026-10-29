# HERO VR 0.0.73 (EXPERIMENTAL release, tag v0.0.73, launcher build 14)

Released as tag v0.0.73 (Version 0.0.73, build Update 39.6, launcher 0.0.73.1, build 14), from test build 6's code
(7af1589) on the branch hero-0.0.73-release.
EXPERIMENTAL: it may crash or have issues; 0.0.63 is the most recent STABLE build (Revert > USE 0.0.63).
The release zip carries `Models/` (with WOLVERINE's claws, `Models/Claws/Left` and `Right`) and `Sounds/`.
Made by Agent Mulder. Only the model artists are credited (THIRD-PARTY-MODELS.md).

## What the release changes from test build 6
- **MP SOLO TEST is left out** (the merge 02c5376 is reverted): no UDP net code, no loopback ghost PLAYER 2, no MP
  Test Buddy, no wrist row, no export 5461, no live.txt `mp_solo_test`. The wrist has 250 controls, 142 options.
- **GAME TAA (live.txt `image_style`) starts OFF.** SPIDYVR stays index 0 of the option and one wrist press away; the
  C++ start value and the launcher default are OFF (as LENS FLARES: index 0 kept, the default differs). GAME AA TAA
  (the game's AA set to TAA with no upscaler while VR runs) stays tied to GAME TAA SPIDYVR, so with the default the
  game's own AA is left alone. `live_settings.migrate_0073` runs once: a live.txt from a test build that holds
  `image_style=spidyvr` moves to `off`, then the `MIGRATED_0073` marker is written; a choice made after the marker
  stays. A test build wrote its default too, so an explicit SPIDYVR picked in a test build also moves to OFF once.
  0.0.72 never wrote `image_style`, so a 0.0.72 live.txt just gets the new default.
- **No test labels**: HERO_TEST_BUILD OFF, so no test notice; the wrist reads "HERO VR 0.0.73 - MADE BY AGENT MULDER"
  and the console "HERO VR 0.0.73 - Made by Agent Mulder: starting OpenXR". Help texts no longer name test builds.

What's new for players: RELEASE-NOTES-v0.0.73.md and CHANGELOG.md. The test-build notes are in docs/IMAGE-STYLE-0.0.73.md,
docs/AA-0.0.73.md and docs/LIGHTS-0.0.73.md.
