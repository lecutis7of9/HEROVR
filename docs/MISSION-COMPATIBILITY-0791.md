# HEROVR 0.0.79.1 — Spider-Man 2 mission-compatibility audit

Experimental branch only. This is not a public release or a claim that these
story beats have been passed inside the headset. Keep the real game's
interactions, targets, completion flags, animations, and quest logic.

## Confirmed native control vocabulary

- **Peter Symbiote Surge / Miles Mega Venom Blast:** L3 + R3 simultaneously.
  The game verifies story unlock and meter. HEROVR's own PAD MODE now uses a
  distinct **hold both physical triggers + L3 + R3** chord (.35 s), and the
  old held-sticks Pause shortcut is suppressed in immersive gameplay.
- **Web Line:** L2 aim + Triangle (Xbox LT + Y). Native L2 + Y is routed
  while Spider-Man is free on ground/perch, left physical trigger aims, and
  Y is pressed. Web Line placement, standing on it and stealth takedown are
  the game's own. This has not been verified in the headset.
- **Stealth takedown:** native Square or Triangle when a stealth target is
  eligible, including targets beneath a Web Line. Existing NamedPress
  preserves the buttons named by the game's mission prompt.
- **Environmental yank/throw:** game's L1 + R1 (Xbox LB + RB). HEROVR already
  has a right-stick-click PullPress route. Experimental contextual grip-yank
  claims the web grip only if the game's L1 + R1 prompt is visible, waits for
  .17 m arm retraction, and sends native LB + RB, reusing existing throw
  aiming. The older physical prop-grab system is **not ported to SM2** and
  remains disabled. This gesture is *not* an independent simulated object
  hanging from a web: it is the first native-action input adaptation.
- **Photo Ops Camera Gadget:** touchpad swipe up to equip, L2 to aim, R1
  shutter, swipe down to put away. **Not** Photo Mode. XInput cannot send
  PlayStation touchpad swipes. The game supports binding *Toggle Camera
  Gadget* to a D-pad shortcut in its own Accessibility > Shortcuts; the
  HEROVR PAD MODE already sends D-pad, LT and RB. No verified default
  no-configuration camera shortcut yet: do not report mission camera fixed.
- **Prowler stash:** hold L2 to scan, then balance independent L2 and R2
  analog pressures in the yellow ranges, then native L1 + R1 to open the
  panel. The PAD MODE forwards genuine independent analog XInput trigger
  values; new offline regression covers this path. No headset mission
  completion test yet.
- **Regular vs Symbiote Peter:** regular Suits menu selection. No fake
  in-combat transformation added.

## Roll Like We Used To — still unresolved

The story's native present-day Peter/Harry bicycle ride transitions to young
Peter inside Midtown High School. This is *not* HEROVR's own summonable
motorcycle (bike075). Do **not** attach a custom motorcycle model or force
the cinematic player to use the mod's vehicle controls. Goals:

1. Keep the actual native bicycle and Peter body visible during the ride,
   with a comfortable world-scale/camera frame.
2. Preserve game-native Young Peter vent, window, and doorway traversal. The
   reported sideways collision near tight openings could be eye
   HeadClearance versus player physics collision; not established yet.
3. Existing session telemetry already records HeadClearance pushed-frame
   count and maximum correction (headPushFrames/headPushMostMm). Compare
   with the scene if a report becomes available. Don't disable all
   collision avoidance on unsupported inference.

## Explicit acceptance tests for an eventual headset build

1. Peter: L3+R3 with Surge ready does not pause or enter PAD, and game Surge
   triggers. Both triggers + L3+R3 held enters PAD exactly once; repeating
   exits. The game menu button still pauses.
2. Web Line tutorial: aim at opposing building, press Y while left trigger
   held; native bridge appears, hero can walk on it, perform Web Line
   takedown, and story progress records it.
3. Normal grip-swing continues when no L1+R1 environmental prompt; when
   prompted, aiming, gripping and yanking an arm uses native throwable and
   doesn't fire a second swing web.
4. Photo Op: native camera gadget opens, subject focus indicator advances,
   R1 shutter completes quest. Photo Mode does not count.
5. Miles Prowler: L2 scan -> two independent analog trigger holds -> native
   L1+R1 door -> stash completion.
6. High school: bicycle and Peter visible in the actual scripted ride; no
   unexpected lateral shove through narrow doors/vents.
7. All above: control changes must leave Dynamic Body, daytime renderer,
   original renderer, and the public v0.0.79 branch untouched.

## Reference guides

- Web Line tutorial: https://dotesports.com/spider-man/news/how-to-use-the-web-line-in-spider-man-2
- Controller moves: https://www.shacknews.com/article/137455/controls-moves-spider-man-2
- Camera Gadget: https://dotesports.com/spider-man/news/how-to-equip-the-camera-in-spider-man-2
- Game accessibility shortcuts: https://support.insomniac.games/hc/en-us/articles/46730041467027-What-Accessibility-options-does-Marvel-s-Spider-Man-2-feature
- Prowler stash analog triggers: https://www.digitaltrends.com/gaming/open-prowler-stash-spider-man-2/
- School story: https://www.pushsquare.com/guides/marvels-spider-man-2-roll-like-we-used-to
