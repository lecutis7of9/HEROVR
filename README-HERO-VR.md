# HERO VR 0.0.6 - Made by Agent Mulder

HERO VR is a free, unofficial VR mod for **Marvel's Spider-Man 2** on PC. It is a port of
**Spidy (SpidyVR) by Ilya Mezerowsky** (https://github.com/IlyaMez/SpidyVR), and it comes with the HERO VR launcher:
one window with a big **PLAY VR** button, launch options, build, updates, revert and reports.

This zip is the full source of mod Version 0.0.6 (build Update 39.1.1) plus the prebuilt HERO VR launcher.

## What's new in 0.0.6
- **Hotfix (build Update 39.1.1, launcher build 8):** if Virtual Desktop refuses the eye images when VR starts ("Create eye swapchain XrResult=-2"), the game no longer falls back to flat screen. It tries again with the 0.0.5 settings, then other settings and smaller sizes, writes every try to the console, and keeps trying. After 2 minutes it tells you to restart Virtual Desktop and the game. The launcher updates itself and then downloads the fixed mod files on its own. If PLAY VR said "No working Python 3.11 or newer was found", you do not need to install anything: open the launcher, let it update (launcher 0.0.6.6), and it puts the mod's own Python back by itself.
- Your 0.0.5 test proved the damage path: blows sent from the mod really hurt enemies. 0.0.6 builds the heroes on it.
- IRON MAN ABILITIES: repulsor blasts and the chest beam (4x a blast, 30 s cooldown on the wrist ring) do real damage,
  and the beam now aims at the enemy you look at.
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
- Don't like 0.0.6? **Revert > USE PREVIOUS VERSION (0.0.5)** downloads 0.0.5 from GitHub, checks it and installs it.
  The launcher then stays on 0.0.5 until you press **Updates > Update now**.

## Requirements
- Windows 10 or 11 (64-bit).
- Marvel's Spider-Man 2 on Steam, **version 2.810.0.0** (Spider-Man2.exe). Other versions are refused on purpose.
- A PC VR headset and its OpenXR app: Virtual Desktop (tested with Quest 3), Quest Link / Air Link or SteamVR.
- **Python 3.11+** from python.org, with "Add python.exe to PATH" ticked (it runs the mod's live settings).
- An internet connection on the first run. You don't need Visual Studio or Git: nothing is compiled on your PC.

## Install
1. Right-click the zip, choose **Extract All...**, and extract it to a short path you can write to, for example
   `C:\HERO-VR`. Don't use Program Files or a OneDrive folder.
2. Double-click **HERO VR Launcher.cmd**. If SmartScreen warns you, click **More info** and then **Run anyway**.
   The launcher is unsigned.
3. On the first run the launcher downloads the ready-made mod files for 0.0.6 from the GitHub release
   (https://github.com/lecutis7of9/HEROVR/releases/tag/v0.0.6) and checks them. If that was interrupted,
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
- Third-party software is listed in THIRD_PARTY_NOTICES.md: OpenXR SDK, JsonCpp, MinHook, Dear ImGui, fonts, bHaptics SDK and Python.
- The HERO VR port and launcher are by Agent Mulder. TikTok: https://www.tiktok.com/@agentmulder, Discord: https://discord.gg/W2b9YjRXzf

## License
HERO VR is free software under the **GNU General Public License version 3**. See LICENSE.
The complete source is in this zip; see SOURCE.txt. You may share and change it under the GPLv3.

Not affiliated with or endorsed by Insomniac Games, Sony Interactive Entertainment, Marvel or Valve.
"Marvel's Spider-Man 2" is used only to say which game this mod is for, and it is a trademark of its owners.
