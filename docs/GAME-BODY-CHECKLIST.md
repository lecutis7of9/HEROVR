# GAME BODY (EXPERIMENTAL) - in-game checklist

Branch `hero-0.0.72-gamebody`. Test build only. Nothing here is released.

**What it is:** a new setting. When it's ON, HERO VR uses the game's own Spider-Man body as your VR body, but only after a set of safety checks pass. If any check fails, you get the hidden hero and gloves instead. The game never crashes and never drops to flatscreen.
**OFF (the default)** = exactly how 0.0.72 works today.

Every game address this uses is **CANDIDATE** (SM2 2.810). None is CONFIRMED until the steps below pass.

## Before you start
- Use Spider-Man 2 PC version 2.810, freshly started, loaded into free roam.
- Run the commands from the repo folder on the gaming PC, with the game window in front.
- You need a build from this branch: the CI artifact `prebuilt-mod`, or `tools/build.ps1 -Observer`.

## 1. The read-only checks (no headset; nothing is injected or hooked)
```
python tools/probe_game_body.py --phases find
```
**Pass** looks like this:
```
FIND: PASS writer 0x292b880 conversion 0x292af50 record 0x...
```
- `(found again by bytes)` at the end is fine. It means the game moved the code and the probe found it again.
- `FIND: FAIL <reason>` means stop here and send the reason. Nothing was changed in the game.

## 2. The body itself (no headset)
```
python tools/probe_game_body.py --phases find,solve
```
**Pass** looks like this:
```
FIND: PASS ...
SOLVE: PASS (wrists [x, y] mm, head z mm, n ms/frame, hero_lag ~0, turn_max t, eye image reports\body-probe\eye-body.png)
```
What the numbers should be:
- **wrists:** both under 10 mm (the hands land on the controllers).
- **head:** under 1 mm. The spec's reference is 0.0 mm.
- **ms/frame:** under 1. The spec's reference is about 0.03.
- **hero_lag:** about 0 (under 0.05 m).
- **turn_max:** note it down; it's from the 4-second walk.

Then open `reports\body-probe\eye-body.png`. It's the left eye looking down: you should see the hero's own body and legs, with no head in the way.

`SOLVE: FAIL missing: ...` or `SOLVE: FAIL protected joint under the body: ...` means the rig doesn't match the spec. That refusal is correct: nothing gets hooked, and the hero stays hidden. Send the line.

The full report goes to `reports\body-probe.json`.

## 3. In the headset
1. Start HERO VR as usual. Open the wrist hologram, go to **BODY AND VIEW**, and set **GAME BODY (EXPERIMENTAL)** to **ON**. No restart is needed.
2. Watch the launcher's VR log, or DebugView (it uses OutputDebugString). Within about a second you should see:
   - `GAME BODY: checking`
   - `GAME BODY: verified (writer 0x..., conversion 0x..., record 0x...)`
   - `GAME BODY: hooked; the body is on`
3. Look down: you should see Spider-Man's body following you, and no head in your view.
4. Load a cutscene or fast-travel. You should get a new `GAME BODY: verified (... record 0x...)` line: it found the player again.
5. **Any `GAME BODY: refused: ...` line** (or `rig refused: ...`) means a check failed. You should then see the hidden hero and gloves, and play goes on. Send the line.
6. **To go back:** set GAME BODY to **OFF**. It works right away and it's the same as today.

## What to send back
- The FIND and SOLVE lines from steps 1 and 2.
- `reports\body-probe.json` and `reports\body-probe\eye-body.png`.
- Any `GAME BODY:` lines from step 3.
