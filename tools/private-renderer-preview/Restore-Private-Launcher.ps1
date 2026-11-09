# HERO VR PRIVATE Original / Revamped launcher preview — safe restore.
param([string]$Root = "$HOME\Downloads\HERO-VR-Launcher-0.0.78\HV078")
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath($Root)
if (Get-Process 'Spider-Man2' -ErrorAction SilentlyContinue) { throw 'Close Spider-Man 2 first.' }
if (Get-Process 'SM2VRLauncher' -ErrorAction SilentlyContinue) { throw 'Close the HERO VR launcher first.' }

$store = Join-Path $Root 'reports\renderer-modes'
$pointer = Join-Path $store 'last-private-backup.txt'
if (-not (Test-Path -LiteralPath $pointer)) { throw 'No private preview backup was found. Nothing was changed.' }
$backup = [IO.File]::ReadAllText($pointer).Trim()
if (-not (Test-Path -LiteralPath (Join-Path $backup 'spidy_stereo_probe.dll'))) {
    throw "Private preview backup is incomplete: $backup"
}

$targetDll = Join-Path $Root 'build\windows-ninja\spidy_stereo_probe.dll'
$targetExe = Join-Path $Root 'tools\launcher\SM2VRLauncher.exe'
Copy-Item -LiteralPath (Join-Path $backup 'spidy_stereo_probe.dll') -Destination $targetDll -Force
$oldExe = Join-Path $backup 'SM2VRLauncher.exe'
if (Test-Path -LiteralPath $oldExe) {
    Copy-Item -LiteralPath $oldExe -Destination $targetExe -Force
} elseif (Test-Path $targetExe) {
    # The stock HERO VR Launcher.cmd rebuilds the exe from source if absent.
    Remove-Item -LiteralPath $targetExe -Force
}
$oldLive = Join-Path $backup 'live.txt'
if (Test-Path $oldLive) { Copy-Item -LiteralPath $oldLive -Destination (Join-Path $Root 'live.txt') -Force }

foreach ($mode in @('original','revamped')) {
    $dest = Join-Path $store "$mode\spidy_stereo_probe.dll"
    $old = Join-Path $backup "$mode-previous.dll"
    if (Test-Path $old) { Copy-Item -LiteralPath $old -Destination $dest -Force }
    elseif (Test-Path $dest) { Remove-Item -LiteralPath $dest -Force }
}
Remove-Item -LiteralPath $pointer -Force
Write-Host ''
Write-Host 'PREVIOUS HERO VR LAUNCHER AND RENDERER RESTORED!' -ForegroundColor Green
Write-Host "Backup: $backup"
Write-Host 'Your previous live.txt settings were restored if they existed.'
