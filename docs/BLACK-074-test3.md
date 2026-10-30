# 0.0.74 test 3: the black headset

Made by Agent Mulder.

## The player's sessions (fa15f5f)

| | test 2b (2cc44bb, worked) | test 3 014414 | test 3 020500 |
|---|---|---|---|
| eye pairs the game rendered (eye_jobs left) | 18716 | 797 | 281 |
| pairs the bridge handed to XR (copy_gpu) | 35623 | 30 | 14 |
| eye picture to headset copy, average / most | 19.6 ms / 1002 ms | 534 ms / 1023 ms | 601 ms / 1008 ms |
| pairs shown (unique_submitted) | 18560 | 0 | 0 |
| presentation | immersive | none | none |
| HEAP GUARD at the eye bridge | (none) | ON | ON (live.txt said off) |
| SPIDYVR STOCK | (none) | on | on |

The game rendered the eyes about 60 times a second (eye_camera as_asked) and the samples' gate was
clear, but the bridge staged a pair only about every 0.6 s. The XR copy then took a pair whose head pose
was no longer in the pose history (`history.find`), so `copied` stayed false: nothing was shown.

## Why live.txt's heap_guard = off did not help

In fa15f5f game_xr sent live.txt's HEAP GUARD (and SPIDYVR STOCK) to the bridge only inside the
`if (copied)` block. No pair was ever shown, so the bridge kept its built-in ON all session.

HEAP GUARD ON lets go of the eye images and refuses every pair begun before the newest eye job each time
the game makes a render heap. While the game streams it makes them often, which matches the pairs
staged every ~0.6 s. This is the leading cause (no heap counter reached the report: the session summary
that prints it needs the bridge's final data).

## The fix

- 1e739f9: the guard acts at most once a second and falls back for the session when the eyes starve
  (45 pairs); live.txt's HEAP GUARD and SPIDYVR STOCK reach the bridge every frame, picture or not.
- The default-OFF commit: HEAP GUARD is OFF by default (value 0 OFF, 1 ON), and the bridge starts OFF, so
  test 3 runs test 2b's heap path unless the player turns the guard on.

SPIDYVR STOCK was on in both black sessions and is not ruled out by these two reports. If the
headset is still black with the fix, the next A/B is `spidyvr_stock = off`.
