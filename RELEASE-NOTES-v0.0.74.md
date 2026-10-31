# HERO VR 0.0.74 - Made by Agent Mulder

## Install in 3 steps
1. **Download `HERO-VR-Launcher-0.0.74.zip`** from *Assets* below (not "Source code": that is only the source).
2. **Right-click it > Extract All...** and open the **HERO-VR-Launcher-0.0.74** folder (if Windows puts it inside
   another folder of the same name, open that one too).
3. **Double-click `HERO VR Launcher.cmd`**, then press **PLAY VR**. That is the only file to open.

> **EXPERIMENTAL BUILD: 0.0.74 may crash or have issues. 0.0.63 is the most recent STABLE build.** You can always go
> back: in the launcher open **Revert** and press **USE 0.0.73** or **USE 0.0.63** (your settings are kept). Before
> updating, the launcher asks you to confirm that you understand this.

**0.0.74: a cleaner picture in the headset (SpidyVR's eye anti-aliasing), lit windows that stay lit at the edges of
your view, new flying, a quicker Daredevil grapple, COLOR and MUSIC tabs on the roster, the Batmobile and hideouts,
and an internet server list for multiplayer.**

VR for Marvel's Spider-Man 2 (Steam, Spider-Man2.exe 2.810.0.0), with the HERO VR launcher (0.0.74.1, build 15).
Version 0.0.74 = build Update 39.7.

**New install:** the 3 steps above. On the first PLAY VR the launcher downloads the ready-made mod files
(`HERO-VR-0.0.74-mod-win64.zip`) and checks their SHA-256; nothing is compiled on your PC.
**Already on 0.0.73 or older:** the launcher offers 0.0.74 by itself (or **Updates > Update now**), after the
EXPERIMENTAL notice above.

## What's new
- **SM1STYLE eye anti-aliasing (on by default):** each eye uses the game's own TAA the way SpidyVR does, so edges
  shimmer much less. To go back to the old look, put `sm1style = off` in live.txt (it applies the next time VR starts).
- **Window-edge lights fixed:** lit windows no longer go dark at the outer edges of each eye.
- **The picture is back to 0.0.72's baseline**, keeping 0.0.73's flicker fixes (no lens flares or eye bloom in VR,
  steadier street lights and shadows, fewer popping far windows). 0.0.73's extra picture switches are gone.
- **Flying:** hold a grip to start flying, steering follows your body and is smoother, flips are livelier, and a tap
  on **Y** while flying looks up ahead when you lie flat. Flying no longer stalls or sinks when a hand blinks out of
  tracking.
- **Daredevil quick grapple:** a faster, smooth pull, and pressing grapple again with either baton mid-pull
  re-aims it at once (Arkham style).
- **Roster:** a **COLOR** tab in the header and a **MUSIC** tab on each hero with the music player.
- **Music kept:** your songs live in Documents\HERO VR\Music, so updates and reverts never lose them.
- **Multiplayer:** an internet **server list** (HERO VR's own free Cloudflare lobby), plus hosting and joining on
  your own network. The list only shows game names, heroes and player counts.
- **Batman:** four drivable **Batmobiles** (1989, ODAMI, FOREVER, FOREVER 2), the **Batcave** with the Batcomputer
  and a **safehouse** for the van, crime alerts with a route on the dash, glide pitch from your controllers' tilt,
  and separate belt spots for the grapple gun, batarangs and smoke pellets.
- **Cars:** the van and Batmobiles are solid on foot, don't show through walls, keep your play area facing forward
  as you get in, and turn flips off while you drive.
- **Throws** fly along your arm's swing (batarangs, shield, hammer, batons). Punisher's hook fires instantly, and no
  grapple press is lost.
- **Launcher (0.0.74.1, build 15):** safety checks (mod check, two-GPU check, clearer errors), Reports include the
  newest crash dump, and **Revert** offers **USE 0.0.73** first, then 0.0.63, 0.0.62, 0.0.6 and 0.0.5.

## Known issues (being worked on for 0.0.75)
- Aliasing is better but still needs work.
- Window edges are much better but not fully fixed yet.

## Going back
- Open **Revert** and press **USE 0.0.73** or **USE 0.0.63** (or 0.0.62, 0.0.6, 0.0.5). The launcher downloads that
  release, checks its SHA-256 and installs it. Your settings are kept. Older releases stay available.

## Files
- `HERO-VR-Launcher-0.0.74.zip`: the launcher, the full source, `Models/` (every model, each with its licence) and
  `Sounds/`.
- `HERO-VR-0.0.74-mod-win64.zip`: the ready-made mod files (built by the "Build prebuilt mod" workflow).
- `version.json`: the launcher's update manifest. `SHA256SUMS.txt` and the `.sha256` files: checksums.
- `collect-reports.ps1`: gathers logs for a bug report.

## Credits and licences
Made by Agent Mulder. Based on **Spidy (SpidyVR) by Ilya Mezerowsky** (https://github.com/IlyaMez/SpidyVR), under the
GNU GPL v3. HERO VR is GPLv3 too: see LICENSE, CREDITS.md and THIRD_PARTY_NOTICES.md. Full changes: CHANGELOG.md.

The 3D models belong to their artists and are credited by name, link and licence in THIRD-PARTY-MODELS.md, CREDITS.md
and the launcher's Credits page; each model folder ships its own `license.txt`. New in 0.0.74: the Batmobiles by
Dreamloft3D, Odami_3D, SorroW and S.York-Designs (CC BY 4.0), and the Batcave stage from Injustice: Gods Among Us
(mobile, NetherRealm Studios / Warner Bros. Games, ripped by Ecelon, The Models Resource). Most models are CC BY 4.0.
Some are **non-commercial**: floh's two Punisher suits (CC BY-NC 4.0), Nathang30's Iron Man Mark 1 (CC BY-NC-SA 4.0,
share-alike) and baton268's JOKER batarang (CC BY-NC-ND 4.0, unmodified). Tigerar1's Batman Year One and Marco.41's
Captain America WW2 suits are under the Sketchfab Standard licence (https://sketchfab.com/licenses). HERO VR is free:
it is never sold, paywalled or ad-supported, and these models must not be used commercially.

Not affiliated with Insomniac Games, Sony, Marvel, DC, Warner Bros. or Valve.
