# HERO VR 0.0.82

Everything else is the same as 0.0.81 (Classic and Performance, same gameplay).

## What's new

**Webs and flips work mid-air during long dives.** Jumping off a tall building, the game's own dive could take over and then web presses and left-stick flips were ignored. Now a web press (grip) ends the game's dive move at once and swings, and the game's dive counts as being in the air, so left-stick flips keep working.

**Longer web anchor range.** A web press now looks up to 140 m for a surface to stick to (was 100 m). Open-air anchors still sit at 100 m, so open-air swings feel the same. Set it in `live.txt` with `swing_anchor_range` (20 to 250).

**Near-miss web snap assist.** If a web would just miss, or would only hold in open air, it snaps to the nearest surface within 10 degrees of where you aimed. It is ON by default. In `live.txt`: `swing_assist = off` turns it off, `swing_assist_deg` (0 to 30) sets the cone.

**Weblines are now OFF by default.** Turn them on in the wrist holo: **VR SETTINGS > BODY AND VIEW > WEBLINES**, then pick **GRIP** or **TWO HAND** under **WEBLINE MODE**. If you had weblines on before, they are switched off once on this update; turn them back on and your choice is kept.

This release has not been tested in a headset.

## Install and update

Download **HERO-VR-Launcher-0.0.82.zip**, extract it into a **new empty folder**, and double-click **HERO VR Launcher.cmd**.

If you already have the HERO VR launcher (0.0.81 or earlier), open it and use **Check for Update**. It will offer 0.0.82.

Need an earlier release? On the first screen, *Use a previous version*.

Spider-Man 2 only (SM1 is paused).
