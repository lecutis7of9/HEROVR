# Putting the launcher into the mod repo

Nothing here has been copied into `C:\sm2-port` / the repo yet. These are the steps.

## 1. Copy files

| From (`launcher385\`) | To (in the repo) |
|---|---|
| `src\*.cs` | `tools\launcher\src\` |
| `assets\background.jpg`, `assets\sm2vr.ico`, `assets\sm2vr.png` | `tools\launcher\assets\` |
| `app.manifest`, `build.ps1`, `build.sh`, `launcher.ini.example`, `README.md`, `NOTICE` | `tools\launcher\` |
| `tools\make-version-json.ps1`, `tools\make_version_json.py` | `tools\` (next to `package.ps1`, `make_update_zip.py`) |
| `PUBLISHING.md`, `THIRD-PARTY-LAUNCHER-GUIDE.md`, `THIRD-PARTY-LAUNCHER-GUIDE.pdf` | `docs\` |
| `docs\version.example.json` | `docs\version.example.json` |

`tools\launcher\build.ps1` builds only the launcher. It is not `tools\build.ps1`, which builds the mod.

Add to `.gitignore`:

```
tools/launcher/out/
tools/launcher/launcher-data/
tools/launcher/launcher.ini
```

## 2. Build it and add a shortcut

```powershell
powershell -ExecutionPolicy Bypass -File tools\launcher\build.ps1
```

`SM2 VR Launcher.cmd` at the repo root, next to `Launch Spidy Game VR.cmd`:

```bat
@echo off
rem Builds the launcher the first time, then starts it.
cd /d "%~dp0"
if not exist "tools\launcher\out\SM2VRLauncher.exe" powershell -NoProfile -ExecutionPolicy Bypass -File "tools\launcher\build.ps1" || (pause & exit /b 1)
start "" "tools\launcher\out\SM2VRLauncher.exe"
```

The exe finds the install by walking up from its own folder until it finds
`tools\launch-game-vr.ps1` (beside `VERSION` and/or `PORT-VERSION`). A launcher inside an install always manages that install. The fallback is
`C:\sm2-port`, and MOD FOLDER can pick another one.

## 3. Optional: ship the exe with releases

Put `SM2VRLauncher.exe` in the update zip and the player zip as `tools\launcher\SM2VRLauncher.exe` and point
the `.cmd` at it. Updating itself is safe: Windows can't overwrite a running exe, so the launcher renames it to
`.old` and the new copy runs next time. Backups and reverts never delete files in the launcher's own folder.

## 4. Optional: a clean stop

STOP VR sends Ctrl+C to the PowerShell that runs `launch-game-vr.ps1`, and the script's own cleanup runs. To get
a stop that doesn't depend on the console, add this to `launch-game-vr.ps1`. The launcher doesn't need it yet.

```powershell
param(..., [string]$StopEvent)          # the launcher would pass -StopEvent SM2VR-Stop-<pid>
# in the wait loop:
if ($StopEvent) { $ev = [Threading.EventWaitHandle]::new($false, 'ManualReset', $StopEvent); if ($ev.WaitOne(0)) { break } }
```

Options the script doesn't declare are greyed out automatically, because the launcher reads the `param()` block.
So new script parameters appear once they're added to `src\LaunchOptions.cs` (one line each).

## 5. Optional: release workflow step

After the zip is built in `.github/workflows/release.yml`, add a step that writes `version.json` and uploads it
with the zips (see PUBLISHING.md):

```yaml
      - name: Release manifest (version.json + .sha256)
        run: |
          ./tools/make-version-json.ps1 -Repo "${{ github.repository }}" -Tag "${{ steps.version.outputs.tag }}" `
            -UpdateZip (Get-Item dist/sm2-vr-*.zip).FullName -SourceZip (Get-Item dist/sm2-full-*.zip).FullName `
            -Out dist/version.json
```

Then add `dist/version.json` and `dist/*.sha256` to the files the release step uploads.
