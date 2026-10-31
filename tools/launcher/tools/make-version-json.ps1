# Writes version.json (release manifest, schema 1) plus a <zip>.sha256 file for each zip, for a GitHub Release.
# Same output as make_version_json.py; this one needs only Windows PowerShell 5.1.
#
#   powershell -ExecutionPolicy Bypass -File tools\launcher\tools\make-version-json.ps1 `
#     -Repo owner/sm2-vr -PortRoot C:\sm2-port `
#     -UpdateZip dist\sm2-vr-0.0.1.zip -SourceZip dist\sm2-full-0.0.1.zip -Out dist\version.json
#
# Versions are semver: the first published release is Version 0.0.1 (internal build 38.5), tag v0.0.1, and each
# later release bumps it (0.0.2, 0.0.3, ...; PUBLISHING.md). Read from the mod folder: VERSION ("0.0.1") first,
# else the semver in PORT-VERSION ("Version 0.0.1 (build 38.5)"). A legacy "Update NN.N" is refused.
#
# Upload version.json, the zips and the .sha256 files to the release (PUBLISHING.md).
param(
  [Parameter(Mandatory = $true)][string]$Repo,
  [string]$Tag,
  [string]$Version,
  [string]$BuildNote,
  [string]$PortRoot = ".",
  [string]$UpdateZip,
  [string]$SourceZip,
  [string]$PackageZip,
  [string]$Date,
  [string]$MinGameVersion = "2.810.0.0",
  [string]$InstallSubfolder = "sm2-port",
  [string]$ChangelogUrl,
  [string]$SourceUrl,
  [string]$StableVersion = "0.0.73",   # 0.0.74 test 10: the EXPERIMENTAL notice's STABLE build (Revert > USE 0.0.73)
  [string]$Out = "version.json",
  [switch]$NoShaFiles
)
$ErrorActionPreference = "Stop"

$Repo = $Repo.Trim().Trim('/')
if ($Repo -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$') { throw "-Repo must be owner/name" }
$semverRx = '(?<![\d.])v?(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?![\d.])'
$noteRx = '\(\s*build\s+([0-9][0-9A-Za-z.\-]*)\s*\)'

function First-Line([string]$text) {
  if ($null -eq $text) { return $null }
  foreach ($l in ($text -split "`r?`n")) { $t = $l.Trim().TrimStart([char]0xFEFF); if ($t) { return $t } }
  return $null
}
function Read-First([string]$path) {
  if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $null }
  return First-Line ([IO.File]::ReadAllText((Resolve-Path -LiteralPath $path).Path))
}
# '0.0.1' / 'v0.0.1' / 'Version 0.0.1 (build 38.5)' -> @(number, note); $null for legacy 'Update 38.5'.
function Parse-Semver([string]$text) {
  if (-not $text) { return $null }
  $note = $null
  if ($text -match $noteRx) { $note = $Matches[1] }
  $rest = ([regex]::Replace($text, $noteRx, '')).Trim()
  if ($rest -match '^\s*update\b') { return $null }
  $m = [regex]::Match($rest, $semverRx)
  if (-not $m.Success) { return $null }
  $n = "$($m.Groups[1].Value).$($m.Groups[2].Value).$($m.Groups[3].Value)"
  if ($m.Groups[4].Success) { $n += "-" + $m.Groups[4].Value }
  return ,@($n, $note)
}
function Zip-Version([string]$path) {
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  $z = [IO.Compression.ZipFile]::OpenRead($path)
  try {
    $entries = @{}
    foreach ($e in $z.Entries) { $entries[$e.FullName.Replace('\', '/')] = $e }
    $prefixes = @("") + @($entries.Keys | Where-Object { $_ -like '*/*' } | ForEach-Object { $_.Split('/')[0] + '/' } | Sort-Object -Unique)
    foreach ($p in $prefixes) {
      $lines = @{}
      foreach ($f in "VERSION", "PORT-VERSION") {
        $e = $entries[$p + $f]
        if ($e) { $rd = New-Object IO.StreamReader($e.Open()); try { $lines[$f] = First-Line $rd.ReadToEnd() } finally { $rd.Dispose() } }
      }
      if ($lines.Count) {
        $v = Parse-Semver $lines["VERSION"]; if (-not $v) { $v = Parse-Semver $lines["PORT-VERSION"] }
        if ($v) { return $v[0] }
        return "legacy:" + $(if ($lines["PORT-VERSION"]) { $lines["PORT-VERSION"] } else { $lines["VERSION"] })
      }
    }
    return $null
  } finally { $z.Dispose() }
}

if ($Version) {
  $parsed = Parse-Semver $Version
  if (-not $parsed) { throw "-Version '$Version' is not semver (MAJOR.MINOR.PATCH, e.g. 0.0.1)" }
} else {
  $vLine = Read-First (Join-Path $PortRoot "VERSION")
  $pLine = Read-First (Join-Path $PortRoot "PORT-VERSION")
  $parsed = Parse-Semver $vLine
  $fromPort = Parse-Semver $pLine
  if ($parsed) { if (-not $parsed[1] -and $fromPort) { $parsed = @($parsed[0], $fromPort[1]) } }
  else { $parsed = $fromPort }
  if (-not $parsed) {
    if ($vLine -or $pLine) { throw "$PortRoot`: '$(if ($vLine) { $vLine } else { $pLine })' is not a semver version (a legacy 'Update NN.N' from before 0.0.1?). Put the release version in VERSION, e.g. 0.0.1." }
    throw "$PortRoot`: no VERSION or PORT-VERSION"
  }
}
$number = $parsed[0]
$note = $(if ($BuildNote) { $BuildNote } else { $parsed[1] })
$Version = "Version $number"
if (-not $Tag) { $Tag = "v$number" }
if ($Tag -ne "v$number") { throw "-Tag $Tag does not match the version: use v$number" }
$base = "https://github.com/$Repo/releases/download/$Tag/"

$files = @()
foreach ($pair in @(@("update", $UpdateZip), @("source", $SourceZip), @("package", $PackageZip))) {
  $kind = $pair[0]; $path = $pair[1]
  if (-not $path) { continue }
  if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Not found: $path" }
  $item = Get-Item -LiteralPath $path
  if ($item.Name -match ' ') { throw "Rename '$($item.Name)': GitHub turns spaces in asset names into dots." }
  $zv = Zip-Version $item.FullName
  if ($zv -and $zv.StartsWith("legacy:")) { throw "$($item.Name) carries the legacy version '$($zv.Substring(7))'; it must say $number (VERSION / PORT-VERSION)" }
  if ($zv -and $zv -ne $number) { throw "$($item.Name) says version $zv, not $number; rebuild the zip from the right folder" }
  if ($kind -ne "package" -and -not $item.Name.Contains($number)) { Write-Warning "$($item.Name) does not have $number in its name (expected sm2-vr-$number.zip / sm2-full-$number.zip)" }
  $hash = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
  $files += [ordered]@{ kind = $kind; name = $item.Name; url = $base + $item.Name; sha256 = $hash; size = $item.Length }
  if (-not $NoShaFiles) {
    [IO.File]::WriteAllText($item.FullName + ".sha256", "$hash  $($item.Name)`n", [Text.Encoding]::ASCII)
  }
  "{0,-8} {1}  {2}" -f $kind, $hash, $item.Name
}
if (-not ($files | Where-Object { $_.kind -in "update", "source" })) { throw "Give -UpdateZip and/or -SourceZip (launchers install those)." }

$credits = @(
  [ordered]@{ name = "Ilya Mezerowsky"; role = "Spidy (SpidyVR), the VR mod this is built on"; url = "https://github.com/IlyaMez/SpidyVR"; support = "https://ko-fi.com/ilyamezerowsky" },
  [ordered]@{ name = "Sebi (sjrl2000)"; role = "SpidyVR Linux (Steam Proton) guide, pull request 7"; url = "https://github.com/sjrl2000" },
  [ordered]@{ name = "iplusdvr"; role = "SpidyVR pull requests 21, 22 and 23"; url = "https://github.com/iplusdvr" },
  [ordered]@{ name = "Adrian (AdrianGzzEsc)"; role = "SpidyVR pull request 24"; url = "https://github.com/AdrianGzzEsc" },
  [ordered]@{ name = "Blazeventura and ANTIOPSVR"; role = "Blaze's Spider-Man for Blade & Sorcery, swinging inspiration"; url = "https://www.nexusmods.com/bladeandsorcery/mods/12564" },
  [ordered]@{ name = "SM2VR 0.0.27"; role = "Marvel's Spider-Man 2 game addresses used by the port" }
)

$manifest = [ordered]@{
  schema            = 1
  name              = "SM2 VR"
  version           = $Version
  version_number    = $number
  tag               = $Tag
  release_date      = $(if ($Date) { $Date } else { (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd") })
  min_game_version  = $MinGameVersion
  install_subfolder = $InstallSubfolder
  files             = $files
  build             = [ordered]@{ script = "tools/build.ps1"; args = @("-Observer") }
  launch            = [ordered]@{ script = "tools/launch-game-vr.ps1"; args = @("-RenderScale", "100", "-OverlayWebs", "-EngineTaa", "-SmallerDesktopView") }
  changelog_url     = $(if ($ChangelogUrl) { $ChangelogUrl } else { "https://github.com/$Repo/releases/tag/$Tag" })
  source_url        = $(if ($SourceUrl) { $SourceUrl } else { "https://github.com/$Repo" })
  license           = "GPL-3.0"
  credits           = $credits
}
if ($StableVersion) {
  if ($StableVersion -notmatch $semverRx) { throw "-StableVersion '$StableVersion' is not semver" }
  $manifest["experimental"] = $true; $manifest["stable_version"] = $Matches[1] + "." + $Matches[2] + "." + $Matches[3]
}
if ($note) { $manifest["internal_build"] = $note }   # informational; launchers order by version_number
$json = ConvertTo-Json $manifest -Depth 6
$outPath = [IO.Path]::GetFullPath([IO.Path]::Combine((Get-Location).Path, $Out))
[IO.File]::WriteAllText($outPath, $json + "`n", (New-Object Text.UTF8Encoding $false))
"wrote $Out"
