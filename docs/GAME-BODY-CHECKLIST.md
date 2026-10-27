# GAME BODY (EXPERIMENTAL) - in-game checklist (test build 4)

Private test build only. Nothing here is released.

**What it is:** a new setting. When it's ON, HERO VR uses the game's own Spider-Man body as your VR body, but only after a set of safety checks pass. If any check fails, you get the hidden hero and gloves instead. The game never crashes and never drops to flatscreen.
**OFF (the default)** = exactly how test build 3 works.

Every game address this uses is **CANDIDATE** (SM2 2.810). None is CONFIRMED until the steps below pass.

All of this runs from your mod folder (the one with `Launch Spidy VR.cmd` in it), for example:
`C:\Users\candl\Downloads\HERO-VR-Launcher-0.0.70\HERO-VR-0.0.70`
It uses the Python that comes with the mod. You don't need to install anything.

## Before tests 1 and 2
- Start Spider-Man 2 **from Steam, not through HERO VR**. Load your save into free roam and stand still on the ground.
- You don't need the headset for these two tests.
- Before test 2, start the game fresh: the probe can only start the mod's parts once per game run.

## 1. The read-only checks (nothing is injected or hooked)
Double-click **`tools\Game Body Test 1 - checks.bat`**.
(Or type it yourself: click the folder's address bar, type `cmd`, press Enter, then run:
`python\python.exe tools\probe_game_body.py --phases find`)

**Pass** looks like this:
```
FIND: PASS writer 0x292b880 conversion 0x292af50 record 0x...
```
- `(found again by bytes)` at the end is fine. It means the game moved the code and the probe found it again.
- `FIND: FAIL <reason>` means stop here and send the reason. Nothing was changed in the game.

## 2. The body itself
Restart the game fresh into free roam (see above). Then double-click **`tools\Game Body Test 2 - body.bat`**.
(Or in a `cmd` window in the mod folder: `python\python.exe tools\probe_game_body.py --phases find,solve`)

It brings the game window to the front and moves the hero by itself for a little while. Don't touch anything until it says Done.

**Pass** looks like this:
```
FIND: PASS ...
SOLVE: PASS (wrists [x, y] mm, head z mm, n ms/frame, hero_lag ~0, turn_max t, eye image ...eye-body.png)
```
What the numbers should be:
- **wrists:** both under 10 mm (the hands land on the controllers).
- **head:** under 1 mm.
- **ms/frame:** under 1.
- **hero_lag:** about 0.
- **turn_max:** just note it down.

Then open `reports\body-probe\eye-body.png`. It's the left eye looking down: you should see the hero's own body and legs, with no head in the way.

`SOLVE: FAIL missing: ...` or `SOLVE: FAIL protected joint under the body: ...` means this suit's skeleton doesn't match. That refusal is correct: nothing gets hooked. Send the line.

**Close the game when it's done**, before you play in VR.

## 3. In the headset
1. Start HERO VR as usual (`Launch Spidy VR.cmd` or the launcher).
2. On the left wrist hologram, open the **COMBAT** page. Set **GAME BODY (EXPERIMENTAL)** (the last row) to **ON**. No restart is needed.
3. Look down: you should see Spider-Man's own body following you, with no head in your view.
4. Fast-travel, or let a cutscene play. The body should come back by itself afterwards.
5. If the body never shows, you get the hidden hero and gloves (same as OFF), and play goes on. That means a check failed.
6. **To go back:** set GAME BODY to **OFF**. It works right away and it's the same as before.

## What to send back
- The FIND and SOLVE lines from tests 1 and 2.
- `reports\body-probe.json` and `reports\body-probe\eye-body.png`.
- After step 3: the newest files in the `reports` folder.
  (The `GAME BODY: ...` lines show in the black Launch Spidy VR window, for example `GAME BODY: verified (...)`, `GAME BODY: hooked; the body is on`, or `GAME BODY: refused: <reason>`.)
