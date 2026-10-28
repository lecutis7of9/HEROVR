# HERO VR 0.0.72 - Made by Agent Mulder

HERO VR is a free, unofficial VR mod for **Marvel's Spider-Man 2** on PC. It is a port of
**Spidy (SpidyVR) by Ilya Mezerowsky** (https://github.com/IlyaMez/SpidyVR), and it comes with the HERO VR launcher:
one window with a big **PLAY VR** button, launch options, build, updates, revert and reports.

This zip is the full source of HERO VR 0.0.72 (version number **Version 0.0.72**, build Update 39.5, launcher 0.0.72.1)
plus the prebuilt HERO VR launcher.

**EXPERIMENTAL BUILD: 0.0.72 may crash or have issues. 0.0.63 is the most recent STABLE build** (Revert > USE 0.0.63
goes back to it, settings kept). 0.0.72 includes everything in 0.0.71.

## What's new in 0.0.72
- **Two new heroes** on the wrist hologram's HERO PROFILES: **THOR** (Mjolnir sheathed on your back: reach back and
  grip to draw it, throw, recall and catch, lightning) and **GREEN LANTERN** (ring flight and constructs). One hero at a
  time, and every hero stays OFF until you pick it.
- **THOR HAMMER** (COMBAT): four real Mjolnir models, #1 BOOOOP (the default), #5 DEVILSEYE, #6 LOVE THUNDER and #3 FCCOOPER.
- **Real 3D models**: the batons, batarangs, guns, grapple gun and shield now use the artists' own geometry, UVs and
  textures, no longer flattened or one colour per triangle. A missing model shows MODEL MISSING, never a stand-in.
- **23 worn suits** (the ROSTER page on the wrist): pick a suit for each hero. A worn suit takes the game body's place
  (SUIT BODY: ARMS, FULL BODY or OFF) and shows in MIRROR VIEW. The default is still THE GAME'S OWN suit.
- **Reputation, skill trees and perks** for every hero.
- **GRAPPLE** for BATMAN, DAREDEVIL and the PUNISHER: takeoff from the ground, a sustained pull and a line drawn.
  BATMAN's trigger fires the grapple, a grip on a belt pouch draws a batarang, and his cape fans out while gliding.
- **SUPERMAN**: two thin heat-vision beams from the eyes, with sparks, a scorch mark and a looping sound.
  **PUNISHER**: drop-leg thigh holsters, the barrel along the aim, a muzzle flash and the player's own shotgun, pistol
  and reload sounds. **CAPTAIN AMERICA**: the thrown shield always flies home onto the forearm.
  **WOLVERINE**: grip-fist claws with SNIKT. **THOR**'s lightning uses the player's two lightning sounds.
- **WINDOW LIGHTS**: LOD FADE is off by default, and the game view's lens is never narrower than both eyes.
- The phone is Spider-Man only, and putting it back in the back pocket works (no stuck phone).
- **PUNISHER VAN (EXPERIMENTAL, OFF by default)**: tap X as the Punisher to summon it. It is an engine preview only:
  the van does not drive you through the city yet.
- The launcher (0.0.72.1, build 13) credits every model and suit artist. Before it updates, it still asks you to
  confirm that the new version is EXPERIMENTAL and that 0.0.63 is the most recent STABLE build. **Revert** offers
  **USE 0.0.63**, 0.0.62, 0.0.6 and 0.0.5.

## In 0.0.71
- Fixed: VR not starting after Stop VR ("handle is invalid" at finding the headset).

Why 0.0.70, 0.0.71 and 0.0.72: versions compare number by number, so "0.0.7" would look older than 0.0.63
to the launcher; 0.0.70 is 0.0.7, and 0.0.71 and 0.0.72 come after it for every launcher.

## What's new in 0.0.7
- **New heroes** on the wrist hologram's COMBAT page, under HERO PROFILES: **BATMAN** (batarangs that chain and come
  back, grapple, smoke), **SUPERMAN** (heat vision, COLD BREATH, super strength, the SUPERSONIC dash), **PUNISHER** (two
  pistols and a sawed-off, holsters, reload), **WOLVERINE** (claws, healing) and **CAPTAIN AMERICA** (his shield: wear,
  draw, guard, throw and bash). One hero at a time. **Every new hero and option is OFF** until you turn it on, and
  SHIELD BLOCK is an OFF-by-default experiment.
- **Guns fire along the barrel now**, from the gun in your palm, and the batons sit in your fist.
- **Optional 3D models** for the batons, batarangs, guns and shield (BATON LOOK, BATARANG STYLE, GUN MODELS, SHIELD
  LOOK). They are in the `Models` folder; a missing file falls back to the built-in look.
- **Mirror View runs once and turns itself off.** The MIRROR VIEW test now runs for its 30 seconds at the next VR
  start, then switches itself OFF (and saves that), so it never stays on by accident.
- **Captain America suit:** `CapSuit` and docs/CAP-SUIT-PIPELINE.md are an optional guide for doing a suit swap by
  hand. Nothing installs it for you.
- The launcher (build 12 in 0.0.71, build 11 in 0.0.70): the HERO VR design's orb is smoother and lags less (still optional; classic stays
  the default). **Revert** offers **USE 0.0.63**, 0.0.62, 0.0.6 and 0.0.5.
- The Valve Index 15-degree controller tilt is **unconfirmed**; tell us how it feels.

## In 0.0.63
- **A new look for the launcher, if you want it:** the HERO VR design, with an animated orb, ambient sound and
  slanted menu items. It is optional. The classic design stays the default; the launcher asks you once whether to
  try the new one, and **Options > Launcher design** switches between the two at any time (the launcher restarts).
- If the new design ever fails to start, the launcher opens the classic one by itself and says why. Hold **Shift**
  while starting the launcher (or start it with `--classic`) to open the classic design.
- The ambient sound stops when you close the launcher or start the game, and the animation pauses while you play.
- **Fixed: 0.0.62 failing to start VR.** In 0.0.62 PLAY VR stopped at "starting the game" with the error
  `name 'session_off' is not defined`. 0.0.63 fixes it. Otherwise the mod is the same as 0.0.62: same features, same
  settings, same defaults.
- **Revert** now offers **USE 0.0.62** as well as 0.0.6 and 0.0.5.

## In 0.0.62
- **Ghost hands:** the EYE AA smoothing no longer leaves faint copies of your hands behind when they move fast.
- **Repulsor bolts, the chest beam and your web balls stop at the first wall** (with their impact flash there)
  instead of flying on through it. No new game address: they use the same ray checks as swinging.
- **Headlights and bridge lights through walls at night:** these are the game's own lens flares. New option
  **LENS FLARES** (GRAPHICS): OFF IN VR turns them off while VR runs (takes effect at the next VR start; GAME, the
  default, leaves them as they are).
- **Daredevil's batons hurt enemies again** when you hit with them in your hands, not only when thrown.
- **Combat feels quicker:** **PUNCH HITS** (COMBAT; GAME + INSTANT by default) also lands a small hit the moment your
  fist touches, like SpidyVR, as well as the game's move. GAME MOVE is the 0.0.6 behaviour. **ENEMY TRACKING**
  (LIVE by default) follows enemies 30 times a second by their own position, so lock-on and punches no longer aim
  where an enemy was while you lunge or swing; 0.0.6 is the old way.
- **HIT INDICATOR** (COMBAT, OFF by default): a brief white X where your punches, batons, blasts and webs land.
- **HAND FLIPS** (COMBAT, ON by default as in 0.0.6): OFF stops arm swings from starting flips; the stick and A
  still flip you.
- **New CHEST BEAM sound** made from the same player's repulsor sound, and the bolts and the chest beam **shimmer like plasma**.
- **Iron Man's hand blast uses your own repulsor sound** (thanks to the player who sent it), **fires from the palm**
  in the direction the palm faces, and hand blasts now have a **2-second cooldown per hand**, shown by a ring on each
  wrist. The CHEST BEAM is separate: 4x a blast's damage, 30-second cooldown shown on the wrist ring.
- **Daredevil's thrown baton bounces** between up to 3 enemies (each within 5 m of the last, never the same one twice)
  and always comes back.
- **REAL ANCHORS** (launcher, WEBS & MOVEMENT, ON by default): webs only hold on a real building or object within
  100 m; a web into empty sky misses with a short buzz. Turn it off for the old swing in thin air.
- **BEACONS IN VISION** (DAREDEVIL, ON by default): mission beacons and markers stay bright and coloured in the sonar.
- The launcher (build 9) can go back to **0.0.6 or 0.0.5** from Revert, and the first run explains what went wrong
  when the zip was not extracted, or antivirus removed the launcher (tools\launcher\build.log).

## In 0.0.6
- **Hotfix (build Update 39.1.1, launcher build 8):** if Virtual Desktop refuses the eye images when VR starts ("Create eye swapchain XrResult=-2"), the game no longer falls back to flat screen. It tries again with the 0.0.5 settings, then other settings and smaller sizes, writes every try to the console, and keeps trying. After 2 minutes it tells you to restart Virtual Desktop and the game. The launcher updates itself and then downloads the fixed mod files on its own. If PLAY VR said "No working Python 3.11 or newer was found", you do not need to install anything: open the launcher, let it update (launcher 0.0.6.6), and it puts the mod's own Python back by itself.
- Your 0.0.5 test proved the damage path: blows sent from the mod really hurt enemies. 0.0.6 builds the heroes on it.
- IRON MAN ABILITIES: hand repulsor blasts and the CHEST BEAM do real damage. The CHEST BEAM does 4x a blast's damage,
  has a 30-second cooldown shown on the wrist ring, and now aims at the enemy you look at. (Since 0.0.62, hand repulsor
  blasts have a 2-second cooldown per hand.)
- DAREDEVIL ABILITIES: a baton in each hand, a trigger throw with strong aim assist that chains to up to 3 more enemies
  within 6 m and always comes back, harder punches (DAREDEVIL PUNCH, 1x to 5x, 3x by default) and a sonar look.
- HULK ABILITIES: a 4x charged jump and one-punch knockouts on normal enemies (bigger ones take 2 to 3).
- One hero at a time; all heroes are OFF by default and none of them needs a game restart.
- A cleaner hologram: 8 pages (STATUS, POWERS, VR SETTINGS, COMBAT, GRAPHICS, COLOR, CONTROLS, MUSIC), each split into
  named sections that open with a press, one at a time. Nothing was removed, only reorganised.
- POWERS page: INVINCIBLE, FLYING (fly mode, supersonic and its options) and HERO PROFILES (Spider-Man, Iron Man,
  Daredevil, Hulk; one hero at a time). It opens on the hero you are playing.
- New Iron Man repulsor: a palm charge, a tight white-hot bolt with a blue falloff and a short impact burst, and a new,
  tighter sound. It costs about the same as the webs (no new render pass).
- Settings that need a restart (EYE SUPERSAMPLE, MIRROR VIEW TEST) carry a gold RESTART tag; everything else is live.
- The report now carries the MIRROR VIEW TEST result, and SUPERSONIC logs the speed each dash reaches.
- Frame rate help: after a session the mod says whether your graphics card is the limit, suggests 72 or 80 Hz in your
  VR app when the game already makes about half your headset's rate (no more judder), and suggests RenderScale 90 or 80
  (about +14% and +30% frames; estimates from one RTX 4080 measurement). AUTO PERFORMANCE (off by default) now aims at
  half rate when full rate is out of reach. The graphics defaults are unchanged.
- VR never ends silently when the headset is lost: the mod waits for it and brings VR back by itself.
- Opt-in reports (see Privacy below).
- Don't like 0.0.72? **Revert > USE 0.0.63**, **USE 0.0.62**, **USE 0.0.6** or **USE 0.0.5** downloads that version from GitHub, checks it and
  installs it. The launcher then stays on it until you press **Updates > Update now**.

## Requirements
- Windows 10 or 11 (64-bit).
- Marvel's Spider-Man 2 on Steam, **version 2.810.0.0** (Spider-Man2.exe). Other versions are refused on purpose.
- A PC VR headset and its OpenXR app: Virtual Desktop (tested with Quest 3), Quest Link / Air Link or SteamVR.
- Nothing else to install: the mod files come with their own Python.
- An internet connection on the first run. You don't need Visual Studio or Git: nothing is compiled on your PC.

## Install
1. Right-click the zip, choose **Extract All...**, and extract it to a short path you can write to, for example
   `C:\HERO-VR`. Don't use Program Files or a OneDrive folder.
   Open the **HERO-VR-Launcher-0.0.72** folder it makes (if Windows puts it inside another folder of the same name,
   open that one too).
2. Double-click **HERO VR Launcher.cmd**: it is the only file to open. If SmartScreen warns you, click **More info** and then **Run anyway**.
   The launcher is unsigned.
3. On the first run the launcher downloads the ready-made mod files for 0.0.72 from the GitHub release
   (https://github.com/lecutis7of9/HEROVR/releases/tag/v0.0.72) and checks them. If that was interrupted,
   press **Repair** on **Status** (or just PLAY VR, which offers it).
4. Start your VR app, put the headset on and press **PLAY VR**. The default launch options are:
   `-RenderScale 100 -OverlayWebs -EngineTaa -SmallerDesktopView`.

To get the wrist hologram (the settings menu) in VR, bring your right hand to your left forearm and pull the right trigger. On the keyboard, press F8.
If something goes wrong, open **Reports** in the launcher and press **Report a bug**, or send the newest
`reports\game-vr-*.json` and `-console.log`.

## Privacy
HERO VR never sends anything by itself: no tracking, no accounts, no servers. Reports are opt-in and you see every word
first. Each one opens a filled-in issue on the mod's GitHub page in your browser, and nothing is sent until you press
*Submit* there (a free GitHub account is needed); close the page and nothing is sent.
- **Crash reports**: after a crash, the next start asks "Send this crash report to help fix it?" (Send report, Not now,
  Don't ask again). You can turn this off in Options > Privacy.
- **Report a bug**: a short description, with or without the mod version, graphics card and VR runtime name.
- **Play stats**: **off** unless you turn it on (asked once at the first start, Options > Privacy). It keeps a short
  summary of your last 10 sessions on your PC (fps, stalls, hero and features used, mod version, graphics card and
  headset name) to send from Reports in one click. Turning it off deletes them. No restart is needed.
- Never included: your name, Windows user or PC name, folder paths, e-mail, IP address, Steam ID or save files.
  (Collect reports, the older zip of the `reports` folder, is not cleaned this way; check it before you share it.)

## Credits
- **Spidy (SpidyVR) by Ilya Mezerowsky**: the VR mod this is built on. https://github.com/IlyaMez/SpidyVR,
  support: https://ko-fi.com/ilyamezerowsky
- SpidyVR contributors:
  - Sebi (sjrl2000), Linux guide, PR 7;
  - iplusdvr, PRs 21, 22 and 23;
  - Adrian (AdrianGzzEsc), PR 24.
- Swinging inspiration: Blaze's Spider-Man for Blade & Sorcery, by Blazeventura and ANTIOPSVR. None of its code is used.
- Game addresses: SM2VR 0.0.27.
- 3D models (0.0.7), CC BY 4.0 unless noted: Digital Goblin (shield), iqbalmulyawan (Captain America suit), Yanez
  Designs (batons), TORI106 (pistol), LiliumLetifer (sawed-off), Eira and Ludus101 (batarangs), baton268 (JOKER
  batarang, CC BY-NC-ND 4.0, unmodified). Links and licences: THIRD-PARTY-MODELS.md and CREDITS.md.
- Third-party software is listed in THIRD_PARTY_NOTICES.md: OpenXR SDK, JsonCpp, MinHook, Dear ImGui, fonts, bHaptics SDK and Python.
- The HERO VR port and launcher are by Agent Mulder. TikTok: https://www.tiktok.com/@agentmulder, Discord: https://discord.gg/W2b9YjRXzf

## License
HERO VR is free software under the **GNU General Public License version 3**. See LICENSE.
The complete source is in this zip; see SOURCE.txt. You may share and change it under the GPLv3.

Not affiliated with or endorsed by Insomniac Games, Sony Interactive Entertainment, Marvel or Valve.
"Marvel's Spider-Man 2" is used only to say which game this mod is for, and it is a trademark of its owners.
