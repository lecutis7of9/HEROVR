# SM2 VR - shareable package

**Unzip, start your VR app, double-click Launch Spidy VR.cmd.**

A VR mod for Marvel's Spider-Man 2 on PC (a port of SpidyVR). This folder has everything it needs, including
its own Python and Microsoft's C++ runtime built into its files, so there is nothing else to install.

## What you need

- **Marvel's Spider-Man 2 on Steam, version 2.810.0.0.** Spidy finds it in any of your Steam libraries. Other
  versions are not supported (the mod reads the game's memory at fixed places, and a game update moves them):
  Spidy says so and stops instead of starting. An Epic Games Store copy of the same version is tried.
- **A PC VR headset and its VR app**, running with the headset connected: Virtual Desktop (tested with a
  Quest 3), Meta Quest Link / Air Link, or SteamVR.
- **Windows 10 or 11, 64-bit.**

## Unzip and run

1. Right-click the zip, choose **Extract All...**, and extract it somewhere you can write, such as
   `Documents` or `C:\Games`. Spaces in the path are fine. Keep every file.
   Spidy warns when the folder is in `Program Files`, in a folder OneDrive syncs, or has a very long path.
2. Start your VR app (Virtual Desktop, Link or SteamVR) and put the headset on.
3. Double-click **Launch Spidy VR.cmd**. It starts the game through Steam and switches it to VR. Leave the
   black console window open; close the game to stop. If VR cannot start, the window says why and stays open.

   If Windows SmartScreen warns about the file, click **More info**, then **Run anyway**. Spidy removes
   Windows' "downloaded from the internet" mark from its own files when it starts.

It starts with Spidy's web strands, the game at 100% of the headset's resolution and eye supersampling off.
Settings you change on the hologram are kept; the ones that need a restart say so.

Do not use a Python of your own: the mod always uses the one in its `python` folder (Python 3.12.10), never the
one on your PC, so an older Python on your PC does not matter. If the window says the package's own Python is
missing or won't start, extract the whole zip again and allow the folder in your antivirus. `tools\Spidy
Launcher.exe` is an optional window with the same settings.

## Music

Put your songs (mp3, wav, flac, m4a, aac, wma or ogg) in **Documents\HERO VR\Music** (made at the first start;
sub-folders are read too). It is outside the mod's folder, so updating, unzipping a new version or reverting never
loses your songs. Songs you left in the mod's own **Music** folder (or an older version's beside it) are copied there
once at the next start, never moved or deleted, and that folder is still read. Pick them on the MUSIC page of the
hologram, the phone or the van. Turn Music down to 0 in the game's Audio settings so it doesn't play over your songs.

## The hologram (settings menu)

- **In VR:** bring your right hand close to your left forearm (within about 15 cm, from the back of the hand
  up the arm) and pull the **right trigger** past halfway. Do the same to close it.
- **On the keyboard:** press **F8**.

It opens only during gameplay, not in the game's menus or cutscenes.

## If something goes wrong

Send back the two newest files from the **reports** folder in the extracted folder:

- `game-vr-<date-time>.json`
- `game-vr-<date-time>-console.log`

If you ran **ADDRESS FINDER** (GRAPHICS page; play normally about a minute), also send the
newest `address-finder-<date-time>.json` and `.txt` from the same folder. Those are
candidates only — they do not change the game by themselves.

A screenshot or a short video of the problem also helps.
