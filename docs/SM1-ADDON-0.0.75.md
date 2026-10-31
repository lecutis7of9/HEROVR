# HERO VR 0.0.75 SM1: Spider-Man 1 in the launcher

Made by Agent Mulder. The launcher now has two games, side by side at the top:

- **SPIDER-MAN 2**: HERO VR, exactly as before.
- **SPIDER-MAN 1**: **iilyn's Spider-Man VR mod, with Mulder's Heroes add-on**.

Picking one switches at once (no restart). The choice is saved (`Game=sm1` / `Game=sm2` in settings.ini; Spider-Man 2
is the default).

## His mod

- **Spidy (SpidyVR) by iilyn (Ilya Mezerowsky)**, GPL-3.0: https://github.com/IlyaMez/SpidyVR
  (support him: https://ko-fi.com/ilyamezerowsky, his Discord: https://discord.gg/x27ZXDpdNt).
- It is downloaded from **his own GitHub releases** (the GitHub API's `browser_download_url`, so the download counts
  on his release), checked against GitHub's size and sha256, and unpacked **unchanged** into
  `launcher-data\SM1\Spidy` (the zip's top folder dropped). A failed download or unpack keeps the old copy.
- It is **never changed**: his launcher (`Spidy Launcher.exe`) opens with all of his graphics, performance and
  anti-aliasing settings, and the player presses PLAY there (it has no command-line play). The HERO VR launcher
  never writes into his folder or his settings.
- Game: Marvel's Spider-Man Remastered (Steam 1817070), Spider-Man.exe v4.630.0.0. Other games are not supported.

## Mulder's Heroes add-on

Separate files (`herovr_sm1_addon.dll`, built from `src/sm1`, shipped in `build\windows-ninja` by
`tools\package.ps1` when present). On PLAY it is copied to `launcher-data\SM1\addon\<sha>\`, its settings go to
`%APPDATA%\HERO VR SM1\addon.ini`, and it is loaded into the game once his VR module is running. Its report is
`%APPDATA%\HERO VR SM1\addon.log` (Spider-Man 1 page > Add-on log).

| Part | State in 0.0.75 |
| --- | --- |
| Host checks (exe name, version, his module names) | CONFIRMED (from his 0.7.0 sources) |
| Settings file, wrist menu state machine, flight motion (pure code) | CONFIRMED (tested on Linux) |
| Drawing the wrist menu in his VR frame, moving the player for flight, heroes' powers and models | CANDIDATE: a no-op until SM1 research finds the game addresses |
| Loading the add-on into Spider-Man.exe | Needs in-game testing on Windows |

## Settings (Spider-Man 1 page)

| Setting | Subtext |
| --- | --- |
| Load Mulder's Heroes add-on | Adds heroes, the wrist menu and flight on top of his mod, never changing it; takes effect the next time you press Play. |
| Wrist menu | Turn your left palm up to open the hero menu; takes effect the next time you press Play. |
| Flight | Lets flying heroes fly in Spider-Man 1; takes effect the next time you press Play. |
| Tell me about his new releases | Looks at his GitHub once when the launcher starts (nothing installs by itself); takes effect the next time the launcher starts. |
| The game picker | Pick a game: switches at once, no restart needed. |

## Missing pieces

Each gets one pop-up that says what is missing and why, with **No** or **Yes, Autosetup**:

- the game: Yes opens Steam's install page;
- his mod: Yes downloads his latest release from his GitHub and unpacks it, then starts it;
- the add-on: Yes gets the ready-made HERO VR mod files (Repair); No starts his mod alone.

At start (Spider-Man 1 picked) the first missing piece is offered once; after a No it is only offered again on PLAY.

## Multiplayer

The server list (https://herovr-lobby.herovr.workers.dev, no change to the worker) shows only the picked game's
games: Spider-Man 2 hosts send protocol 1, Spider-Man 1 is the band 128 + protocol (129). Hosting and joining from
Spider-Man 1 say "not hooked up yet" until the add-on is tested in game.
