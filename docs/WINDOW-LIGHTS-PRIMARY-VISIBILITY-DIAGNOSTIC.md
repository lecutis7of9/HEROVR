# HERO VR — distant lit windows, head-turn pop (isolated diagnostic)

## Symptom (owner headset report, November 10, 2026)
Many building windows do not illuminate until close approach. Previously lit
windows can extinguish while the player turns the HMD, despite the building
remaining present. This is **not** established to be TAA, texture sharpness,
or a lighting-material fault.

## What this experimental branch actually changes
This branch starts from the **older 0.0.78 window-visibility experimental
branch**, NOT public 0.0.79 and NOT the combined 0.0.79.1 gameplay preview.
Its existing candidate only adjusts the *primary game view* impostor query
distance (bounded to 150%); never the separate eye queries. Its
`window_visibility = 0` live setting permits A/B rollback.

The November 10 change is **read-only probe instrumentation**, not a visual
fix. The probe uses a spare flag (512) in each existing 376-byte sample to
record `primary_reach_eligible` only when a non-null query result belongs to
the primary game view, the candidate is enabled, the primary view is not
resting, and the known executable-byte guards pass. The Python
`tools/window_probe.py` decoder exposes the flag.

IMPORTANT: `primary_reach_eligible=true` does **not** prove a +0x88 write,
that the engine chose the lit window model, or that the window rendered.
Existing `farWindowsWrites` counters must be used for writes; the probe does
not yet count actual lit-window instances. A missing eligible flag narrows the
diagnosis, but is not enough alone to justify rewriting engine offsets.

## Controlled headset A/B procedure (only after CI compiles)
1. Keep public 0.0.79 and gameplay 0.0.79.1 untouched. Use an isolated
   installation of this experimental **0.0.78-based** renderer DLL.
2. At a saved nighttime location, note a distant building with
   visibly dark windows. Turn the HMD left/right **without moving**.
3. With `window_visibility = 0`, record 10–15 seconds and collect the
   window-probe JSONL and any available reach-write telemetry.
4. Toggle `window_visibility = 1` (experimental +50% primary-view query
   candidate), repeat at the **same position**, gaze angles, lighting,
   renderer settings and time of day.
5. Compare brightness on/off frequency and apparent illumination distance
   while checking `primary_reach_eligible` for the game view, actual writes,
   view orientation, and performance. Repeat for both eyes and at a second
   building if feasible.
6. If the query is eligible/written yet windows still turn off as the head
   rotates, investigate which full-model or emissive-window instances the
   game submits per eye. Do **not** increase all LODs, modify all view
   frusta, force global lights, or reuse an unsafe per-eye +0x88 rewrite.

## Release gate
Passing Windows CI means only compile/offline tests passed. This branch
is not merged and must not be published. It cannot be called a window
fix until a headset A/B verifies stable lit windows and acceptable FPS.
