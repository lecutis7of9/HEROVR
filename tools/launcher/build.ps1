# Builds SM2VRLauncher.exe with the C# compiler that ships with Windows (.NET Framework 4.8). Nothing to install.
#   powershell -ExecutionPolicy Bypass -File build.ps1            -> out\SM2VRLauncher.exe
#   powershell -ExecutionPolicy Bypass -File build.ps1 -Run       -> build, then start it
#   powershell -ExecutionPolicy Bypass -File build.ps1 -Preview   -> build, then start it with fake data (no game needed)
param([switch]$Run, [switch]$Preview, [string]$Out = "out\SM2VRLauncher.exe")
$ErrorActionPreference = "Stop"
Set-Location -LiteralPath $PSScriptRoot   # -LiteralPath: a folder like "HERO-VR [1]" is not a wildcard
# Keep what the compiler said next to the launcher, so a player can send it (tools\launcher\build.log).
try { Start-Transcript -LiteralPath (Join-Path $PSScriptRoot "build.log") -Force | Out-Null } catch { }

$csc = @(
  "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe",
  "$env:WINDIR\Microsoft.NET\Framework\v4.0.30319\csc.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $csc) { throw "csc.exe from .NET Framework 4.x was not found. It ships with Windows 10/11; turn on '.NET Framework 4.8 Advanced Services' in Windows Features." }

New-Item -ItemType Directory -Force -Path (Split-Path $Out) | Out-Null
$src = Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot "src") -Filter *.cs | ForEach-Object { $_.FullName }
if (-not $src) { throw "tools\launcher\src has no .cs files. Unzip the whole HERO VR zip again, into a new folder." }
$refs = "System.dll","System.Core.dll","System.Drawing.dll","System.Windows.Forms.dll","System.IO.Compression.dll",
        "System.IO.Compression.FileSystem.dll","System.Web.Extensions.dll","System.Management.dll" | ForEach-Object { "/r:$_" }

& $csc /nologo /target:winexe /optimize+ /platform:anycpu /langversion:5 "/out:$Out" `
  /win32icon:assets\sm2vr.ico /win32manifest:app.manifest `
  /resource:assets\background.jpg,background.jpg `
  /resource:assets\hero\orb.bin,hero.orb.bin /resource:assets\hero\intro.wav,hero.intro.wav /resource:assets\hero\loop.wav,hero.loop.wav `
  /resource:assets\hero\Barlow-Regular.ttf,hero.Barlow-Regular.ttf /resource:assets\hero\Barlow-Medium.ttf,hero.Barlow-Medium.ttf @refs @src | ForEach-Object { Write-Host $_ }   # through Write-Host so build.log has the compiler messages
if ($LASTEXITCODE -ne 0) { throw "The launcher did not compile (csc exit $LASTEXITCODE)." }
Write-Host "Built $Out ($([math]::Round((Get-Item -LiteralPath $Out).Length/1KB)) KB)" -ForegroundColor Green

if ($Preview) { $env:SM2VR_LAUNCHER_PREVIEW = "1" }
if ($Run -or $Preview) { Start-Process -FilePath (Resolve-Path -LiteralPath $Out).ProviderPath }
