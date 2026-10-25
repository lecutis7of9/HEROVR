param([switch]$CoreOnly,[switch]$Observer,[switch]$FromSource,[ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference = 'Stop'
# Hotfix 2: players never compile. Run by a launcher (an older one runs this after every install), this
# installs the release's ready-made mod files instead (never compiles); -FromSource builds from source.
if ($env:SM2VR_LAUNCHER -eq '1' -and -not $FromSource -and -not $CoreOnly) {
    & (Join-Path $PSScriptRoot 'install-prebuilt.ps1')
    $prebuiltCode = $LASTEXITCODE
    if ($prebuiltCode -ne 3) { exit $prebuiltCode }
    [Console]::Error.WriteLine('The ready-made mod files for this version are not on GitHub yet, so there is nothing to install. Nothing was changed and nothing is wrong with your PC: try again later (players never need to build). Developers: tools\build.ps1 -FromSource.')
    exit 1
}
$playerRun = ($env:SM2VR_LAUNCHER -eq '1')
function Stop-Plain([string]$dev) {
    if ($playerRun) {
        [Console]::Error.WriteLine('The ready-made mod files for this version could not be found, and building them from source needs developer tools (git, Visual Studio C++). Nothing is wrong with your PC: press Repair in the launcher, or update the launcher, and it downloads them.')
        [Console]::Error.WriteLine('(developer detail: ' + $dev + ')')
        exit 1
    }
    throw $dev
}
# Update 31 (since 30.1): the update being built, first and in green.
$portVersionFile = Join-Path (Split-Path -Parent $PSScriptRoot) 'PORT-VERSION'
$portVersion = if (Test-Path -LiteralPath $portVersionFile) { (Get-Content -LiteralPath $portVersionFile -TotalCount 1).Trim() } else { 'unknown (PORT-VERSION missing)' }
$publicFile = Join-Path (Split-Path -Parent $PSScriptRoot) 'VERSION'
$publicVersion = if (Test-Path -LiteralPath $publicFile) { (Get-Content -LiteralPath $publicFile -TotalCount 1).Trim() } else { 'unknown' }
Write-Host "SM2 VR Version $publicVersion (build $portVersion)" -ForegroundColor Green
Write-Host 'HERO VR - Made by Agent Mulder' -ForegroundColor Green
# Version 0.0.5: a fresh folder (the zip unpacked anywhere) has no third_party yet. Name what is
# missing and fetch it with tools/bootstrap.ps1 (the pinned revisions) before building.
$tpRoot = Join-Path (Split-Path -Parent $PSScriptRoot) 'third_party'
$needed = [ordered]@{ 'OpenXR-SDK' = 'CMakeLists.txt'; 'imgui' = 'imgui.cpp'; 'tact-cpp2' = 'sdk\lib\x64\bhaptics_library.dll' }
if ($Observer) { $needed['minhook'] = 'CMakeLists.txt' }
$missing = @($needed.Keys | Where-Object { -not (Test-Path -LiteralPath (Join-Path (Join-Path $tpRoot $_) $needed[$_])) })
$missingTools = @('tools\capture_game_state.py', 'tools\live_settings.py', 'tools\update37_records.py', 'CMakeLists.txt') |
    Where-Object { -not (Test-Path -LiteralPath (Join-Path (Split-Path -Parent $PSScriptRoot) $_)) }
if ($missingTools) {
    Stop-Plain "This folder is missing files the zip ships: $($missingTools -join ', '). Unpack the whole Version $publicVersion zip again."
}
if ($missing.Count) {
    Write-Host "Missing third_party: $($missing -join ', '). Fetching them now with tools\bootstrap.ps1 (needs git and the internet)." -ForegroundColor Yellow
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        Stop-Plain "git is needed to fetch third_party ($($missing -join ', ')). Install Git for Windows, then run tools\build.ps1 again."
    }
    if ($Observer) { & (Join-Path $PSScriptRoot 'bootstrap.ps1') -Observer } else { & (Join-Path $PSScriptRoot 'bootstrap.ps1') }
    $still = @($needed.Keys | Where-Object { -not (Test-Path -LiteralPath (Join-Path (Join-Path $tpRoot $_) $needed[$_])) })
    if ($still.Count) { throw "tools\bootstrap.ps1 ran but third_party is still missing: $($still -join ', ')." }
    Write-Host "third_party ready: $($missing -join ', ')." -ForegroundColor Green
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
# vcvars64.bat runs vswhere by name, and its folder is not on every PATH.
$env:PATH = (Split-Path -Parent $vswhere) + ';' + $env:PATH
if (-not (Test-Path -LiteralPath $vswhere)) { Stop-Plain 'Visual Studio 2022 or 2026 C++ build tools are required (vswhere.exe not found).' }
$vs = & $vswhere -version '[17.0,19.0)' -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { Stop-Plain 'Visual Studio 2022 or 2026 C++ build tools are required (Desktop development with C++).' }
$build = if($CoreOnly){'core-ninja'}else{'windows-ninja'}
$xr = if($CoreOnly){'OFF'}else{'ON'}
$observerFlag = if($Observer){'ON'}else{'OFF'}
# Update 34.1: a file dated in the future (Update 34's zip was) makes ninja stop with "manifest
# 'build.ninja' still dirty after 100 tries": such files are dated now first, so the build always runs.
$root = Split-Path -Parent $PSScriptRoot
$now = Get-Date
$future = @(Get-ChildItem -LiteralPath $root -Recurse -File -Force -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch '\\\.git\\' -and $_.LastWriteTime -gt $now.AddMinutes(1) })
foreach ($f in $future) { try { $f.LastWriteTime = $now } catch { Write-Host "Could not re-date $($f.FullName): $_" -ForegroundColor Yellow } }
if ($future.Count) { Write-Host "Dated $($future.Count) files from the future to now (ninja needs that)." -ForegroundColor Yellow }
& (Join-Path $PSScriptRoot 'build-native.cmd') $vs $build $Configuration $xr $observerFlag
if ($LASTEXITCODE) {
    $code = $LASTEXITCODE
    # Update 27.4: name the failed tests at the very end, so one screenshot shows them.
    $log = Join-Path (Split-Path -Parent $PSScriptRoot) "build\$build\Testing\Temporary\LastTest.log"
    $fails = @()
    if (Test-Path -LiteralPath $log) {
        $fails = @(Select-String -LiteralPath $log -Pattern '^FAIL ' | ForEach-Object { $_.Line.Trim() } | Select-Object -Unique)
    }
    Write-Host ''
    Write-Host '==================== FAILED TESTS ====================' -ForegroundColor Red
    if ($fails.Count) { $fails | ForEach-Object { Write-Host $_ -ForegroundColor Red } }
    else { Write-Host '(no FAIL lines found: the build itself failed, or see the CTest output above)' -ForegroundColor Red }
    Write-Host '======================================================' -ForegroundColor Red
    throw "Build or tests failed (exit $code)"
}

# Update 34.1: the VR module the launcher loads, with its time, so a screenshot shows the build is new.
$vrModule = Join-Path $root "build\$build\spidy_stereo_probe.dll"
if (-not $CoreOnly) {
    if (Test-Path -LiteralPath $vrModule) {
        Write-Host "Built $portVersion VR module: $vrModule ($((Get-Item -LiteralPath $vrModule).LastWriteTime))" -ForegroundColor Green
    } else { throw "The build finished but $vrModule is missing." }
}

# Update 19: the folder the wrist hologram's MUSIC page plays songs from (MP3, WAV, FLAC; the launcher
# and the mod make it too, and it is never emptied).
$music = Join-Path (Split-Path -Parent $PSScriptRoot) 'Music'
if (-not (Test-Path -LiteralPath $music)) { New-Item -ItemType Directory -Path $music | Out-Null }
Write-Output "Music folder (drop songs here): $music"

# Optional Authenticode signing of the shipped launcher and modules. A no-op unless a certificate is
# configured, so unsigned dev builds keep working; signing is what most reduces antivirus false
# positives (an unsigned binary that attaches to a game starts with no reputation). Configure one of:
#   $env:SPIDY_SIGN_THUMBPRINT  SHA-1 thumbprint of a code-signing cert in the Windows store
#                               (a hardware token, or an imported PFX)
#   $env:SPIDY_SIGN_PFX         path to a PFX file (+ $env:SPIDY_SIGN_PASSWORD if it has one)
#   $env:SPIDY_SIGN_ARGS        raw signtool 'sign' arguments, for schemes the two above do not cover
#                               (e.g. Azure Trusted Signing's '/v /debug /dlib <dll> /dmdf <json>')
# Override the RFC3161 timestamp server with $env:SPIDY_SIGN_TIMESTAMP_URL (timestamping keeps the
# signature valid after the certificate expires).
if (-not $CoreOnly -and ($env:SPIDY_SIGN_THUMBPRINT -or $env:SPIDY_SIGN_PFX -or $env:SPIDY_SIGN_ARGS)) {
    $bin = Join-Path (Split-Path -Parent $PSScriptRoot) "build\$build"
    # bhaptics_library.dll is bHaptics' and ships unmodified: it is not signed here.
    $names = @('spidy_launcher.exe', 'spidy_headset_probe.exe', 'spidy_bridge.dll', 'spidy_render_probe.dll',
               'spidy_ray_bridge.dll', 'spidy_movement_bridge.dll', 'spidy_stereo_probe.dll', 'spidy_render_memory.dll',
               'spidy_haptics_test.exe')
    $files = foreach ($n in $names) { $p = Join-Path $bin $n; if (Test-Path -LiteralPath $p) { $p } }
    if (-not $files) { throw "Signing was requested but no built binaries were found in $bin." }

    $signtool = (Get-Command signtool.exe -ErrorAction SilentlyContinue).Source
    if (-not $signtool) {
        $kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
        $signtool = Get-ChildItem -LiteralPath $kits -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '\\x64\\' } | Sort-Object FullName |
            Select-Object -Last 1 -ExpandProperty FullName
    }
    if (-not $signtool) { throw 'signtool.exe not found. Install the Windows SDK (it ships signtool.exe).' }

    if ($env:SPIDY_SIGN_ARGS) {
        $signArgs = @('sign') + ($env:SPIDY_SIGN_ARGS -split '\s+' | Where-Object { $_ })
    } else {
        $ts = if ($env:SPIDY_SIGN_TIMESTAMP_URL) { $env:SPIDY_SIGN_TIMESTAMP_URL } else { 'http://timestamp.digicert.com' }
        $signArgs = @('sign', '/fd', 'sha256', '/tr', $ts, '/td', 'sha256')
        if ($env:SPIDY_SIGN_THUMBPRINT) {
            $signArgs += @('/sha1', $env:SPIDY_SIGN_THUMBPRINT)
        } else {
            $signArgs += @('/f', $env:SPIDY_SIGN_PFX)
            if ($env:SPIDY_SIGN_PASSWORD) { $signArgs += @('/p', $env:SPIDY_SIGN_PASSWORD) }
        }
    }
    & $signtool @signArgs @files
    if ($LASTEXITCODE) { throw 'signtool failed to sign the binaries.' }
    & $signtool verify /pa @files | Out-Null
    if ($LASTEXITCODE) { throw 'The signed binaries did not verify.' }
    Write-Output "Signed and verified $($files.Count) binaries."
}
