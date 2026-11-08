param([switch]$BuildOnly)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build\windows-ninja'
$package = Join-Path $root 'build\HEROVR-0.0.78-Diagnostic-1'
$zip = Join-Path $root 'build\HEROVR-0.0.78-Diagnostic-1.zip'
Write-Host 'HEROVR 0.0.78 diagnostic: compile, run tests, package' -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'build.ps1') -FromSource -Configuration Release
if ($LASTEXITCODE -ne 0) { throw 'Build failed; no diagnostic package created.' }
$required = @('spidy_stereo_probe.dll', 'spidy_bridge.dll', 'spidy_render_probe.dll',
              'spidy_launcher.exe')
foreach ($name in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $build $name))) {
        throw "Missing expected build output: $name"
    }
}
if (Test-Path -LiteralPath $package) { Remove-Item -LiteralPath $package -Recurse -Force }
New-Item -ItemType Directory -Path $package | Out-Null
Get-ChildItem -LiteralPath $build -File |
    Where-Object { $_.Extension -in @('.dll', '.exe') } |
    Copy-Item -Destination $package
$tools = Join-Path $package 'diagnostics'
New-Item -ItemType Directory -Path $tools | Out-Null
foreach ($name in @('analyze_camera_history.py','check_motion_vectors.py')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $tools
}
@'
HEROVR 0.0.78 Diagnostic Build 1 (experimental)
Built from the experiment branch. Not a public release.
The camera history recorder writes herovr-camera-history-<PID>.csv
beside the loaded spidy_stereo_probe.dll. Capture up to 240 jobs per eye.
After one VR session, analyze the CSV with:
  py -3 diagnostics\analyze_camera_history.py herovr-camera-history-<PID>.csv

IMPORTANT: This is a diagnostic package, not a standalone game installer.
Use the normal HEROVR launcher/install procedure for your existing setup.
No GPU G-buffer constant buffer is captured by this build; the
check_motion_vectors.py script requires a separate 576-byte GPU capture.
Do not run two different mod builds at the same time.
'@ | Set-Content -LiteralPath (Join-Path $package 'DIAGNOSTIC-README.txt') -Encoding UTF8
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -LiteralPath (Join-Path $package '*') -DestinationPath $zip
Write-Host "Package ready: $zip" -ForegroundColor Green
if (-not $BuildOnly) {
    Write-Host 'The ZIP is ready. Start your existing launcher as usual after installing the diagnostic binaries.'
}
