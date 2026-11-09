HERO VR 0.0.78 - PRIVATE RENDERING MODE LAUNCHER PREVIEW
============================================================

THIS IS NOT A PUBLIC RELEASE.
It is a private test artifact of the Original / Revamped selector.

1. Close Spider-Man 2 and your old HERO VR launcher.
2. Extract this whole ZIP to a new folder in Downloads.
3. Double-click INSTALL-PRIVATE-LAUNCHER.cmd. It checks SHA256 of:
       - new launcher exe
       - official v0.0.78 ORIGINAL renderer DLL
       - successfully headset-tested REVAMPED Clarity v2 DLL
4. The updated launcher opens itself. Both options are visible in
   the opening header: ORIGINAL | REVAMPED (private test).
5. Changing selection just remembers your choice; your active renderer
   is swapped before PLAY VR starts with the game closed.
6. The very first install does NOT replace your current rendering DLL.
7. After changing mode, close the game before trying the other mode.
8. This does not publish a GitHub release, update users, or alter main.

RESTORE:
Close the game and launcher, run RESTORE-PRIVATE-LAUNCHER.cmd.
Or run Restore-Private-Launcher.ps1 with PowerShell Bypass.

Current user install path by default:
%USERPROFILE%\Downloads\HERO-VR-Launcher-0.0.78\HV078

Custom locations:
powershell -ExecutionPolicy Bypass -File .\Install-Private-Launcher.ps1 -Root "X:\Path\To\HV078"

Important: Existing private builds/launchers in a different folder are
not affected. For public release, change the default selection to ORIGINAL
and separately validate all launcher and story-mode functionality.

Source: https://github.com/lecutis7of9/HEROVR/tree/experiment/078-private-launcher-original-revamped
