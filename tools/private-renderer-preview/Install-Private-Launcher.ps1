# HERO VR PRIVATE Original / Revamped launcher test — install into existing 0.0.78.
# Run from the extracted PRIVATE zip, not while Spider-Man 2 or the launcher is open.
param([string]$Root = "$HOME\Downloads\HERO-VR-Launcher-0.0.78\HV078")
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function MustExist([string]$Path, [string]$Why) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "$Why : $Path" }
}
function CopyChecked([string]$Src, [string]$Dst) {
    $folder = Split-Path -Parent $Dst
    New-Item -ItemType Directory -Path $folder -Force | Out-Null
    Copy-Item -LiteralPath $Src -Destination $Dst -Force
    $a = (Get-FileHash -LiteralPath $Src -Algorithm SHA256).Hash
    $b = (Get-FileHash -LiteralPath $Dst -Algorithm SHA256).Hash
    if ($a -ne $b) { throw "Copy failed checksum verification: $Dst" }
}

$Root = [IO.Path]::GetFullPath($Root)
$payload = Join-Path $PSScriptRoot 'payload'
$expected = @(
    'payload/SM2VRLauncher.exe',
    'payload/original/spidy_stereo_probe.dll',
    'payload/revamped/spidy_stereo_probe.dll'
)
$hashes = Join-Path $PSScriptRoot 'sha256.txt'
MustExist $hashes 'Private preview manifest missing (extract the entire ZIP)'
foreach ($line in [IO.File]::ReadAllLines($hashes)) {
    $bits = $line -split '\s{2,}', 2
    if ($bits.Count -ne 2 -or $bits[0] -notmatch '^[0-9a-fA-F]{64}$' -or $bits[1] -notin $expected) {
        throw "Unexpected private preview manifest entry: $line"
    }
    $file = Join-Path $PSScriptRoot $bits[1]
    MustExist $file 'Preview package is incomplete'
    $got = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
    if ($got -ne $bits[0]) { throw "Checksum mismatch; no files installed: $file" }
}
if ([IO.File]::ReadAllLines($hashes).Length -ne 3) { throw 'The private preview must contain exactly three checked payload files.' }

$rootMarker = Join-Path $Root 'tools\launch-game-vr.ps1'
$targetDll = Join-Path $Root 'build\windows-ninja\spidy_stereo_probe.dll'
$targetExe = Join-Path $Root 'tools\launcher\SM2VRLauncher.exe'
MustExist $rootMarker 'HERO VR 0.0.78 was not found'
MustExist $targetDll 'The existing working rendering DLL was not found'
if (Get-Process 'Spider-Man2' -ErrorAction SilentlyContinue) { throw 'Close Spider-Man 2 before installing.' }
if (Get-Process 'SM2VRLauncher' -ErrorAction SilentlyContinue) { throw 'Close the HERO VR launcher before installing.' }

$backups = Join-Path $Root 'reports\renderer-modes'
$backup = Join-Path $backups ('private-launcher-backup-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath $targetDll -Destination (Join-Path $backup 'spidy_stereo_probe.dll')
if (Test-Path $targetExe) { Copy-Item -LiteralPath $targetExe -Destination (Join-Path $backup 'SM2VRLauncher.exe') }
$live = Join-Path $Root 'live.txt'
if (Test-Path $live) { Copy-Item -LiteralPath $live -Destination (Join-Path $backup 'live.txt') }
foreach ($mode in @('original', 'revamped')) {
    $old = Join-Path $backups "$mode\spidy_stereo_probe.dll"
    if (Test-Path $old) {
        Copy-Item -LiteralPath $old -Destination (Join-Path $backup "$mode-previous.dll")
    }
}

CopyChecked (Join-Path $payload 'SM2VRLauncher.exe') $targetExe
foreach ($mode in @('original', 'revamped')) {
    $source = Join-Path $payload "$mode\spidy_stereo_probe.dll"
    $dest = Join-Path $backups "$mode\spidy_stereo_probe.dll"
    CopyChecked $source $dest
}

# No game DLL is replaced until the player actually presses PLAY VR with
# Original or Revamped selected. The current stable DLL stays installed now.
[IO.File]::WriteAllText((Join-Path $backups 'last-private-backup.txt'), $backup, [Text.UTF8Encoding]::new($false))
Write-Host ''
Write-Host 'HERO VR PRIVATE LAUNCHER PREVIEW INSTALLED!' -ForegroundColor Green
Write-Host 'The CURRENT VR rendering DLL has NOT been changed.'
Write-Host 'Original / Revamped are visible when the launcher opens.'
Write-Host "Backup: $backup"
Write-Host ''
Write-Host 'Starting the launcher now...'
Start-Process -FilePath $targetExe -WorkingDirectory (Split-Path -Parent $targetExe)
