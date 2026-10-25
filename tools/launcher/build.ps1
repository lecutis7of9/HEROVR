# Builds SM2VRLauncher.exe with the C# compiler that ships with Windows (.NET Framework 4.8). Nothing to install.
#   powershell -ExecutionPolicy Bypass -File build.ps1            -> out\SM2VRLauncher.exe
#   powershell -ExecutionPolicy Bypass -File build.ps1 -Run       -> build, then start it
#   powershell -ExecutionPolicy Bypass -File build.ps1 -Preview   -> build, then start it with fake data (no game needed)
param([switch]$Run, [switch]$Preview, [string]$Out = "out\SM2VRLauncher.exe")
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$csc = @(
  "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe",
  "$env:WINDIR\Microsoft.NET\Framework\v4.0.30319\csc.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $csc) { throw "csc.exe from .NET Framework 4.x was not found. It ships with Windows 10/11; turn on '.NET Framework 4.8 Advanced Services' in Windows Features." }

New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null
$src = Get-ChildItem src\*.cs | ForEach-Object { $_.FullName }
$refs = "System.dll","System.Core.dll","System.Drawing.dll","System.Windows.Forms.dll","System.IO.Compression.dll",
        "System.IO.Compression.FileSystem.dll","System.Web.Extensions.dll","System.Management.dll" | ForEach-Object { "/r:$_" }

& $csc /nologo /target:winexe /optimize+ /platform:anycpu /langversion:5 "/out:$Out" `
  /win32icon:assets\sm2vr.ico /win32manifest:app.manifest `
  /resource:assets\background.jpg,background.jpg @refs @src
if ($LASTEXITCODE -ne 0) { throw "The launcher did not compile (csc exit $LASTEXITCODE)." }
Write-Host "Built $Out ($([math]::Round((Get-Item $Out).Length/1KB)) KB)" -ForegroundColor Green

if ($Preview) { $env:SM2VR_LAUNCHER_PREVIEW = "1" }
if ($Run -or $Preview) { Start-Process (Resolve-Path $Out) }
