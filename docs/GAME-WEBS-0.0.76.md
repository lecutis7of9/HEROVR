# HERO VR 0.0.76 (mpworld): GAME WEBS ONLY and Spider-Man 1 paused

Made by Agent Mulder.

## GAME WEBS ONLY (live.txt `game_webs`, export SpidyGameWebs, live id 5556)

`game_webs = on` (the default) applies to Spider-Man in Spider-Man 2 only. HERO VR draws none of its own webs; you see only
the game's own web lines (from your wrists), the game's own web balls and the game's own webbed-up enemies.

Not drawn while it is on (the logic behind each one is unchanged):

- HERO VR's swing and zip lines, the two-hand held piece and the let-go animation. If the game's lines are not running
  (for example `game_webs` was turned on mid-session after an overlay-webs start), HERO VR's swing line still shows,
  so a web is never invisible.
- The GRAB WEB rope. The game has no line for it, so the grab works but shows no rope.
- The wall-stick nets.
- The cocoons on webbed enemies. Their hold and damage still work; the game's own webbed-enemy effect shows.
- The OURS web ball with its splash, and the GLOB. While the switch is on, BALL LOOK OURS and GLOB play as WRIST: the
  game's own web ball, fired from your wrist with the same aim. The wrist HUD still shows your own BALL LOOK.

Kept: swing input, the swing state, web zips, the attach points, the wrist anchoring of the game's lines
(native webs), and web-ball aim and firing.

The game's web lines start at session start when `game_webs` is on, even with the launcher's SPIDY WEB STRANDS
(--overlay-webs) choice. `game_webs = off` brings back HERO VR's own webs; the swing lines need a restart for that.

Other heroes are not affected. Their tethers (the hero chain and hook lines, the batarang, the shield, the THOR and GL
chains, the DD and PUNISHER grapples) are separate draw code, not the game's web system. A multiplayer friend's web
balls are still drawn by HERO VR, because the game has no ball to show for them.

## Live ids

suit_probe is 5555 and game_webs is 5556 (5553 and 5554 are the flight branch's fly_classic and double_health).

## Spider-Man 1 paused

- The launcher no longer shows the SPIDER-MAN 2 / SPIDER-MAN 1 picker, and a saved Spider-Man 1 pick reads as
  Spider-Man 2 (`Sm1.Paused` in tools/launcher/src/Sm1.cs). All the Spider-Man 1 code stays.
- The test zip does not ship the SM1 add-on (`$shipSm1 = $false` in tools/package.ps1). The build-prebuilt zip check
  follows that line.
- To bring Spider-Man 1 back, set `Sm1.Paused` to false and `$shipSm1` to `$true`.
