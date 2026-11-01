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
| Wrist hero menu | Opens exactly like the SM2 build: touch or point at your left wrist/forearm with the right hand and pull the right trigger (the same click closes it; the SM2 code, wrist_hud::Toggle, is reused unchanged). The palm-up gesture is gone. It has the SM2 hologram look (red cut-corner frame, spider, blue lines on dark glass); right stick moves, right trigger picks. Pages HEROES, OUTFITS, POWERS, ADD-ON. Drawn as an extra quad layer on his finished frame. | built, not tried in game |
| Hero select | 11 heroes; the pick is saved to `addon.ini` (`hero=`). | built, not tried in game |
| Outfits | Each hero's outfits with artist and licence; saved (`outfit.N=`). Spider-Man 1 shows **no hero models yet**. | built, not tried in game |
| Holo card | The hero's card on the wrist (2D). | built, not tried in game |
| Hero powers | Trigger of an outstretched hand (more than 0.42 m from the head) fires the hero's power at the enemy his combat has ringed, through his `SpidyPunchTest`; a beam in the hero's colour. | built, not tried in game |
| Flight | Iron Man, Superman, Thor, Green Lantern: hold a grip 0.4 s to take off, you stay up after letting go. Left stick moves relative to where you look, right stick up/down climbs, a held grip boosts along that hand. Touching ground after 1 s in the air lands. | built, not tried in game |
| Powers on every enemy | The powers hit every enemy type the game lists (read-only enemy list), each blow through `SpidyPunchTest` on its own worker thread in order; HULK hits an area, THOR's lightning chains. | built, not tried in game |
| Grapple | CAPTAIN AMERICA and DAREDEVIL (live.txt `grapple=1`): hold a grip and pull its trigger to fire a line at a wall or roof (2.5 to 80 m, his `SpidyAimSample` preview), it pulls you there and vaults you over the edge; let go of the grip to coast; press **A** while pulled to jump out (DAREDEVIL's JUMP OUT). The SM2 build's pull, tunes, coast and jump-out code, unchanged. | built, not tried in game |
| COLOR | The SM2 build's COLOR / ATMOSPHERE under the heroes on the wrist's HEROES page (the same page, not a separate tab): look (NONE, VIVID, GOLDEN, NEON, RAIMI, NOIR, ANIMATED, METROPOLIS, SMALLVILLE, GOTHAM, CYBERPUNK, SYNTHWAVE) and the sliders STRENGTH, EXPOSURE, CONTRAST, SATURATION, VIBRANCE, WARMTH, TINT, VIGNETTE. Drawn by the layer as its own pass on his two eye images when he releases them (one copy and one fullscreen draw per eye on his graphics queue, the SM2 grade shader). **OFF by default**: with it off (or a neutral grade) there is no GPU work at all, so his picture, anti-aliasing and performance are exactly as without the add-on. His own overlays already in the eye image (webs, HUD panels) are graded too. ANIMATED is its colour grade only (the SM2 cel outline pass is not in SM1). | built, not tried in game |
| Hero models | Needs Spider-Man 1's character model slots. | **not done** |
| Held props (Thor's hammer, Cap's shield, Wolverine's claws) | Left out: the layer only draws flat quads on top of his frame and gets no depth from the game, so a 3D prop would float over walls and hands; a real version needs a 3D model renderer in the layer that is depth-tested against the game. | **not done** |

While the menu is open the right trigger and stick go to the menu only; a hero's power trigger and a flying hero's
grips are hidden from his mod, so his webs never fire by mistake. Spider-Man (hero 0) leaves every input to his mod.

## His exports the layer uses

No game address of our own is used by any feature (the research scan below only reads them). All are his exported functions, read by name in his 0.7.0 source:

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
| Wrist menu | Touch your left wrist with your right hand and pull the right trigger to open or close the hero menu, as in SM2; takes effect the next time you press Play. |
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
| `grapple` | CAPTAIN AMERICA and DAREDEVIL grapple instead of web swinging: hold a grip and pull its trigger to fire a line at a wall or roof, let go of the grip to coast, press A to jump out; applies live, no restart. |
| `color_fx` | COLOR: 1 draws the colour grade on both eyes, 0 (default) leaves iilyn's picture exactly as it is with no extra GPU work; applies live, no restart. |
| `color_filter` | The look by name (`none` ... `synthwave`) or number 0 to 11; applies live, no restart. |
| `color_strength`, `color_exposure`, `color_contrast`, `color_saturation`, `color_vibrance`, `color_warmth`, `color_tint`, `color_vignette` | The SM2 build's sliders, same ranges and steps; applies live, no restart. |

On the wrist menu's HEROES page the heroes come first, then the COLOR rows, each in two columns: stick up/down
picks a row; on a COLOR row a left or right flick (or the trigger) steps its value, and the row's subtext is shown
under the rows; the last row goes on to the next page. A failure in the colour pass turns only COLOR off (the row
then shows OFF (SEE LOG)) and leaves his picture as it is.

The menu's FLIGHT toggle is for this session only; the launcher's Flight switch is the saved one.

## First in-game test

1. Pick SPIDER-MAN 1, press PLAY VR (his VR starts directly with his launcher.ini settings; OPEN IILYN'S LAUNCHER changes them).
2. Open `%APPDATA%\HERO VR SM1\layer.log`: it says whether the layer started, which formats it chose and which
   features turned off and why. `addon.log` says whether `herovr_sm1_layer.dll` was loaded.
3. Right hand on the left wrist, pull the right trigger: the menu. Pick IRON MAN, hold a grip: take off.
4. Pick CAPTAIN AMERICA or DAREDEVIL, hold a grip and pull its trigger at a wall: the line pulls you; press A to jump out.
5. On the HEROES page, move down past the heroes to the COLOR rows, flick right on COLOR to turn it on, then flick through the looks (GOTHAM, SYNTHWAVE ...).
   layer.log says `COLOR: the colour pass is ready` and `first eye image graded`.
6. Change a suit in the game's Suits menu and drive or walk close to cars, so the research scan's dump mode records
   the suit and vehicle components.
7. Stay in VR with the game running (swinging is fine) for at least 3 minutes after the player is found, so the
   eye targets pass runs at least twice (the first pass once his eyes are there, then every 60 s, 6 at most). The
   scan's `.txt` then says `eye targets: passes N; the eye depth location: CONFIRMED record N (name)`, or
   `CANDIDATE records ...`, or `not found`; layer.log has a `sm1scan: eye targets pass N logged (...)` line per pass.

## Research scan (live.txt `sm1scan`, on by default)

A one-session research log for the next SM1 features: held props, enemies and damage, grapple anchors, vehicle /
seat / spawn classes. It is **read only**: it copies memory (VirtualQuery-checked, each copy under SEH) and his
exported *data* (SpidyXrData, SpidyStrikeData, SpidyMotionData, SpidyBodyPoses, SpidyRope/Swing/Grab/Throw...); it
never writes game memory and never calls a game function or one of his functions (its one call is described under
`eyeTargets` below). One lowest-priority thread, small
chunks with 1 ms sleeps, a 48 MB cap, and it ends by itself 15 minutes after the player is found. `sm1scan=0` in
live.txt pauses it at once. A fault ends the scan only.

Output: `%APPDATA%\HERO VR SM1\scan\sm1scan-<date>-<time>.jsonl` (one JSON object per line), `.bin` (raw dumps,
referenced as `[offset, length]`) and `.txt` (a summary); the newest 5 sessions are kept. The launcher's Reports
zip (tools/collect-reports.ps1) adds the newest 2 sessions and layer.log, addon.log, live.txt, addon.ini.

Every value it writes is **CANDIDATE**. The RVAs come from SpidyVR 0.7.0's sources (include/spidy/sm1_scan.hpp
names the file for each) for 4.630.0.0; the scan records what is really there so each can be checked offline.

| Line `kind` | What it records |
| --- | --- |
| `image` | exe base, SizeOfImage, timestamp, checksum, sections, file version; his modules and data exports (address, magic, version, size) |
| `anchor` | each of his RVAs: 64 bytes, a match with his own entry-byte check, vtable RTTI names and first 16 functions, data globals' values and 256 bytes of what they point at |
| `signature` | where his entry-byte patterns occur in the code (up to 8 hits each) |
| `rttiClass` | MSVC RTTI classes whose names match the research areas (Vehicle, Seat, Driver, Spawn, Prop, Attach, Rope, Web, Anchor, Bot, Health, Damage, Hero, Mover, Physics...), with locator and vtable RVAs and first 12 functions |
| `census` | the component registry (his game_player.cpp walk): count, class histogram with RTTI and base classes |
| `playerComponents`, `botComponents`, `spawnComponents` | each followed actor's components (class, object, raw first bytes once) and position |
| `snapshot` (every 2 s, more while webs change) | his head/hands and player record, his motion position/grounded, the player transform, HeroHealth +0x88, the nearest 48 bots (position, distance, health +0x88), spawn-class positions, the hand joints (his SpidyBodyPoses copy and the game's own pose buffer), the HeroRopeManager dump when his web data changes |
| `rig` | the hero's joint table (hashes, parents) with the hand joints found by name hash |
| `hit`, `hitTarget` | when his strike counters move: the strike record, the target's record and health/mover components at 0, 600 and 1500 ms |
| `hisData` | raw copies of his other data exports when they change |
| `eyeTargetTable` | dump mode: the game's render target table (RVA 0x609b060, 132 entries of 0x30 bytes, raw in the .bin) with each entry's kind (low byte of its flags) and the first printable name an entry points at |
| `eyeTargets` | dump mode, one per eye per pass: the eye view (from his SpidyStereoData), its render and texture size, his colour chain check, and its 132 render target records (view +0x1f80 +0x14a0 + i*0x1e8): the record's size (+0x1cc), the textures it points at, each texture's D3D12 format / width / height / flags, a verdict (`depthAtEyeSize`, `depthOtherSize`, `atEyeSize`, `other`, `noDesc`) and the record's 0x1e8 raw bytes |

The eye targets pass makes the scan's one call: the D3D12 runtime's `ID3D12Resource::GetDesc` (a const getter, not a
game or SpidyVR function), only on an object whose vtable is exactly that of one of his swapchain images (an
ID3D12Resource the runtime made), under SEH; otherwise it records the raw bytes only. The session verdict is
CONFIRMED only when exactly one record held a depth-format texture at the eye's render size on both eyes, with a
different resource per eye, in every pass (2 passes at least); anything less stays CANDIDATE.
