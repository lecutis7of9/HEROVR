# HERO VR Launcher

HERO VR is a one-window Windows launcher for the Marvel's Spider-Man 2 VR port. It has one big **PLAY VR** button, the launch
options as toggles and sliders, and pages for build, update, revert, reports and credits. It sits over the red and silver web on the
night city background.

![HERO VR launcher](preview.png)

**HERO VR by Agent Mulder** ([TikTok @agentmulder](https://www.tiktok.com/@agentmulder) · [Discord](https://discord.gg/W2b9YjRXzf)).
*Built on Spidy by Ilya Mezerowsky* (SpidyVR, GPLv3). See the launcher's Credits page and [NOTICE](NOTICE).

Launcher version **0.0.6.3** (launcher build 5), shipped with mod Version 0.0.6 (build Update 39.0.3).

## Privacy and reports

The launcher never sends anything on its own. There is no tracking, no account and no server of ours: every report
opens a pre-filled issue on the mod's GitHub page ([lecutis7of9/HEROVR](https://github.com/lecutis7of9/HEROVR/issues))
in your browser, where you can read and edit all of it, and nothing is sent until **you** press *Submit* (a free
GitHub account is needed for that). Close the page and nothing is sent.

* **Crash reports** (on by default, Options > Privacy > *Offer crash reports*). If the game or the launcher crashed,
  or a VR session did not end cleanly, the next start asks *"Send this crash report to help fix it?"* with
  **Send report**, **Not now** and **Don't ask again**. It is asked once per crash.
* **Report a bug** (Reports). Type a short description; tick or untick the system info (Windows version, graphics
  card, VR runtime name).
* **Play stats** (off by default, asked once at first start, Options > Privacy > *Share play stats*). When on, the
  launcher keeps a short summary of your last 10 VR sessions on your PC, and **Send play stats** in Reports shares
  them in one click: mod version, minutes played, game fps (average and low), VR frame rate, stalls, the hero and
  which features were on, render scale, graphics card and headset/runtime name. Turning it off deletes what was kept.
  Neither setting needs a restart of the game or the launcher.

What is never included: your name, Windows user name or PC name, folder paths (`C:\Users\<you>\...` becomes
`<user>`), e-mail addresses, IP addresses, Steam IDs, save data or files. Reports are plain text you can see before
sending. **Collect reports** is separate and older: it zips `reports\` for you to attach yourself, and those log
files can contain folder paths with your Windows user name, so look before you share it.

## Where it lives
The prebuilt exe is `tools\launcher\SM2VRLauncher.exe`, and **HERO VR Launcher.cmd** in the install's top folder starts it.
A launcher inside an install always manages that install. It finds the install by walking up to `tools\launch-game-vr.ps1`.

## Build it yourself (Windows, nothing to install)
```powershell
powershell -ExecutionPolicy Bypass -File tools\launcher\build.ps1
```
This compiles `src\*.cs` with the C# compiler that ships with Windows (.NET Framework 4.8) into `tools\launcher\out\SM2VRLauncher.exe`.
`-Preview` starts it with sample data, so no game is needed.
On Linux or macOS, `sh build.sh` makes a test build with Mono.

## Settings
- Your settings are saved in `%APPDATA%\SM2 VR Launcher\settings.ini`. The folder keeps its internal name so that settings from earlier
  builds still work.
- `launcher.ini` beside the exe sets these defaults (see `launcher.ini.example`):
  - `UpdateRepo=owner/repo` is the GitHub repository whose Releases carry `version.json`;
  - `CheckOnStart`;
  - `PortRoot`;
  - `Portable`.
- The release manifest is made by `tools/make_version_json.py` (or `tools/make-version-json.ps1`). See PUBLISHING.md.

Internal names are kept on purpose: `SM2VRLauncher.exe`, `%APPDATA%\SM2 VR Launcher`, the `SM2VR-Launcher/` user agent, the
`sm2-vr-*.zip` asset names and the `SM2 VR` manifest name. Only the names you see on screen say HERO VR.

## License
GNU GPL version 3 (LICENSE at the top of the install). Not affiliated with Insomniac Games, Sony Interactive Entertainment, Marvel or Valve.
