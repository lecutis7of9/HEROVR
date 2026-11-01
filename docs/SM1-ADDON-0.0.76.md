# HERO VR 0.0.76 SM1 (hotfix 0.0.75.2): Mulder's Heroes add-on inside Spider-Man 1

Made by Agent Mulder. On top of **iilyn's Spider-Man VR mod**: Spidy (SpidyVR) 0.7.0 by Ilya Mezerowsky, GPL-3.0,
https://github.com/IlyaMez/SpidyVR. His files, his settings (`%APPDATA%\Spidy`), his graphics, performance and
anti-aliasing are not changed. Hero outfits are credited to their model artists in the wrist menu and in
THIRD-PARTY-MODELS.md.

**State: built and wired, NOT yet tried in the game.** The build box has no game; every part below runs its logic in
the Linux tests, and the layer is compiled for Windows by CI. The first in-game test decides what really works.

## How it gets into the game

0.0.75's add-on only wrote a report. 0.0.76 adds `herovr_sm1_layer.dll`, an **OpenXR implicit API layer**:

1. PLAY (Spider-Man 1, add-on on) copies `herovr_sm1_layer.dll` and `herovr_sm1_layer.json` to
   `launcher-data\SM1\layer\<sha>\` and registers the manifest under
   `HKCU\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit` (this Windows user only, never HKLM). Old entries of ours are
   removed first.
2. His launcher starts the game; when his VR module creates its OpenXR instance, the OpenXR loader puts the layer
   between his code and the headset runtime. In any other program the layer passes everything straight through.
3. When Spider-Man 1 closes (or does not start within 15 minutes) the launcher removes the entry. A launcher that
   closed while the game ran removes a leftover entry at its next start.

The OpenXR loader ignores HKCU layers when the game runs as administrator: then the add-on is simply absent.
`HEROVR_SM1_LAYER_DISABLE=1` in the environment turns the layer off.

## What it does

| Feature | How | State |
| --- | --- | --- |
| Wrist hero menu | Left palm up opens it over the left wrist; right stick moves, right trigger picks. Pages HEROES, OUTFITS, POWERS, ADD-ON. Drawn as an extra quad layer on his finished frame. | built, not tried in game |
| Hero select | 11 heroes; the pick is saved to `addon.ini` (`hero=`). | built, not tried in game |
| Outfits | Each hero's outfits with artist and licence; saved (`outfit.N=`). Spider-Man 1 shows **no hero models yet**. | built, not tried in game |
| Holo card | The hero's card on the wrist (2D). | built, not tried in game |
| Hero powers | Trigger of an outstretched hand (more than 0.42 m from the head) fires the hero's power at the enemy his combat has ringed, through his `SpidyPunchTest`; a beam in the hero's colour. | built, not tried in game |
| Flight | Iron Man, Superman, Thor, Green Lantern: hold a grip 0.4 s to take off, you stay up after letting go. Left stick moves relative to where you look, right stick up/down climbs, a held grip boosts along that hand. Touching ground after 1 s in the air lands. | built, not tried in game |
| Hero models | Needs Spider-Man 1's character model slots. | **not done** |

While the menu is open the right trigger and stick go to the menu only; a hero's power trigger and a flying hero's
grips are hidden from his mod, so his webs never fire by mistake. Spider-Man (hero 0) leaves every input to his mod.

## His exports the layer uses

No game address of our own is used. All are his exported functions, read by name in his 0.7.0 source:

| Export | Module | Use | Evidence |
| --- | --- | --- | --- |
| `SpidyXrData` (data) | spidy_stereo_probe.dll | head and hands in game space (XrData v43, 1312 bytes, seqlock) | CONFIRMED |
| `SpidyStrikeSample` | spidy_ray_bridge.dll | the ringed enemy (Strikes v3, 328 bytes: cueTarget, cuePoint) | CONFIRMED |
| `SpidyPunchTest` | spidy_ray_bridge.dll | one blow through the game's damage system (Test v1, 80 bytes) | CONFIRMED |
| `SpidyMotionSubmit` | spidy_movement_bridge.dll | flight velocity (Command v1, 48 bytes, serial must rise, lease up to 250 ms) | CONFIRMED |
| `SpidyMotionSample` | spidy_movement_bridge.dll | grounded / telemetry (MotionData v4, 176 bytes) | CONFIRMED |

The names, sizes and magic numbers are CONFIRMED from his source. That the calls behave in the running game as his
source says is not yet seen (no game on the build box).

His movement module refuses a serial that is not above the last one, and his swing turns itself off after a
refusal. So the layer hooks his two movement exports (MinHook on the exported functions, never on game code) and
hands over: one rising serial order for both of us, his commands held while a hero flies, his telemetry given his
own serials back.

## Fail safe

Every step that fails writes `%APPDATA%\HERO VR SM1\layer.log` and turns off only that feature (INPUT, WRIST MENU,
DRAWING, POWERS, FLIGHT); his calls then pass through unchanged. If the runtime refuses the extra quads, his own frame
is sent again unchanged and drawing stays off. The layer never fails his `xrCreateInstance` or `xrEndFrame`.

## Settings

Launcher (Spider-Man 1 page), written to `%APPDATA%\HERO VR SM1\addon.ini`; the hero and outfits picked in the wrist
menu are kept:

| Setting | Subtext |
| --- | --- |
| Load Mulder's Heroes add-on | Adds heroes, the wrist menu and flight on top of his mod, never changing it; takes effect the next time you press Play. |
| Wrist menu | Turn your left palm up to open the hero menu and pull the right trigger to pick; takes effect the next time you press Play. |
| Flight | Iron Man, Superman, Thor and Green Lantern take off on a held grip and stay up after letting go; takes effect the next time you press Play. |

New switches live only in `%APPDATA%\HERO VR SM1\live.txt` (written with their subtexts on first run, read again every
2 seconds):

| Key | Subtext |
| --- | --- |
| `powers` | Hero powers: the trigger of an outstretched hand fires the hero's power at the ringed enemy; applies live, no restart. |
| `hero_webs` | Heroes who do not fly keep iilyn's web swinging on the grips (the flying heroes use the grips to fly); applies live, no restart. |
| `beams` | Draws each power's beam from the hand (or the eyes) to the enemy; applies live, no restart. |
| `power_damage` | How hard the powers hit, 25 to 200 percent; applies live, no restart. |
| `menu_size` | The wrist menu's size, 60 to 160 percent; applies live, no restart. |

The menu's FLIGHT toggle is for this session only; the launcher's Flight switch is the saved one.

## First in-game test

1. Pick SPIDER-MAN 1, PLAY, press PLAY in his launcher.
2. Open `%APPDATA%\HERO VR SM1\layer.log`: it says whether the layer started, which formats it chose and which
   features turned off and why. `addon.log` says whether `herovr_sm1_layer.dll` was loaded.
3. Left palm up: the menu. Pick IRON MAN, hold a grip: take off.
