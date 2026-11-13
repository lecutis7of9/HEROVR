HERO VR 0.0.82 WEBLINE MODES - TEST 1 (built on 0.0.81; not a release)
=====================================================================

Wrist holo: VR SETTINGS > BODY AND VIEW
  WEBLINES      ON / OFF   master switch (0.0.81). OFF = no Web Line gesture of any mode, grips only swing.
  WEBLINE MODE  GRIP > WRISTS > TWO HAND   (one press cycles; GRIP is the default)
  WEB LINE MARKER  ON / OFF   unchanged (disc / red cross / preview line).

live.txt
  webline_gesture = on | off | anywhere   (master switch; the WEBLINES row)
  webline_mode    = grip | wrists | twohand   (default grip; the WEBLINE MODE row)
  webline_hold_ms = 150..2000   (WRISTS hold, default 400)

All modes end in the same input 0.0.81 sends: the game's own Web Line, L2 held then L2 + Y. Spider-Man 2
decides whether a line can go and WHERE from ITS OWN aim (your head/camera). The anchors / aim below only
decide WHEN the request is sent; they are NOT handed to the game. Nothing here places a line between two
chosen points: the game only offers "from the player toward its own aim point".

GRIP    0.0.81 exactly: grip on a surface, sweep, release on a second surface at least 2 m away.
WRISTS  Bring both controllers together (within 12 cm; they may drift to 17 cm), grips AND triggers released.
        A ring at the wrists fills over webline_hold_ms (clockwise from the top; white, then green when full
        and a surface is valid, red when full with none). Valid = one hand's aim ray is on a surface a web
        holds on, within 35 m (same test as the marker). Hands parting cancels (no input). With no valid
        surface the full ring waits 1.5 s, then cancels. One line per meeting: part the wrists to re-arm.
        L2 is held (the game's aim primes) while the ring runs. No grip is ever taken in this mode.
TWO HAND  Tap a grip (down and up within 0.5 s) aimed at a surface: anchor A (green marker locked there,
        light tick). Tap the OTHER hand's grip on a second surface at least 2 m from A within 3 s: the
        line is requested. Same hand twice moves A. Too close or 3 s without B cancels. L2 is held while A
        waits. Ground / perch / wall only (air only with webline_gesture = anywhere), out of combat.

Limits (honest)
  - Not tested in a headset. A and B (and the WRISTS aim) are gates, not targets; the game aims its own line.
  - TWO HAND still uses the grip like GRIP does on the ground: a grip press aimed at a surface is taken by the
    gesture (the swing does not see it) until let go. Only WRISTS leaves grips completely alone.
  - WRISTS and TWO HAND fire only when deliberate; a grip alone never sends a line in either mode.
  - Holding L2 for the aim also shows the game's own aim view while the ring runs / A waits.
