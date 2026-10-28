# HERO VR 0.0.72 PRIVATE TEST BUILD installer (build 5; build 11 names the wrist line and the folders NOT updated). Started by INSTALL-TEST-BUILD.bat.
#  1. Checks the files beside it (payload\, every one against SHA256SUMS.txt): a part zip not extracted is named.
#  2. Finds every HERO VR mod folder (a folder with tools\launch-game-vr.ps1) in Downloads, Desktop, Documents
#     and C:\sm2-port, and picks the one the game was played from last (its newest report).
#  3. Copies the test build into it (the files it replaces are kept in _before-0.0.72-test first),
#     checks every copied file's sha256, and writes HERO-VR-TEST-BUILD.txt (the console's test line).
#  4. Starts THAT folder's launcher, so the launcher and the game use this folder and no other.
# Settings (live.txt) and reports are never touched. Nothing is deleted.
param([string]$Target = '', [switch]$Yes, [switch]$NoLaunch)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$payload = Join-Path $here 'payload'
function Say($t, $c = 'Gray') { Write-Host $t -ForegroundColor $c }
function Fail($t) { Say ''; Say "NOT INSTALLED: $t" 'Red'; exit 1 }

$marker = Join-Path $payload 'HERO-VR-TEST-BUILD.txt'
if (-not (Test-Path $marker)) { Fail "the payload folder is missing. Extract ALL the part zips into the same folder first (right-click > Extract All, same destination), then run this again from that folder." }
$title = (Get-Content $marker -TotalCount 1)
Say "=== $title ===" 'Cyan'

# 1. The download itself
$sums = Join-Path $payload 'SHA256SUMS.txt'
$entries = @()
foreach ($line in Get-Content $sums) {
    if ($line -match '^([0-9a-f]{64})\s+(.+)$') { $entries += ,@($matches[1], $matches[2].Trim()) }
}
$missing = @(); $bad = @()
foreach ($e in $entries) {
    $p = Join-Path $payload ($e[1] -replace '/', '\')
    if (-not (Test-Path -LiteralPath $p)) { $missing += $e[1]; continue }
    if ((Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLower() -ne $e[0]) { $bad += $e[1] }
}
if ($missing.Count) {
    $models = @($missing | Where-Object { $_ -like 'Models/*' }).Count
    Fail ("$($missing.Count) files are missing" + $(if ($models) { " ($models of them in Models\ - extract part 2 and part 3 into this same folder)" } else { '' }) + ". First missing: $($missing[0])")
}
if ($bad.Count) { Fail "$($bad.Count) files are damaged (download again): $($bad[0])" }
Say "Download OK: $($entries.Count) files, every sha256 matches." 'Green'

# 2. The mod folder
function Played($dir) {
    $r = Join-Path $dir 'reports'
    $t = (Get-Item -LiteralPath $dir).LastWriteTime
    if (Test-Path $r) { $n = Get-ChildItem -LiteralPath $r -File -Recurse -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1; if ($n) { $t = $n.LastWriteTime } }
    return $t
}
$found = @{}
if ($Target) {
    if (-not (Test-Path (Join-Path $Target 'tools\launch-game-vr.ps1'))) { Fail "$Target is not a HERO VR mod folder (no tools\launch-game-vr.ps1)." }
    $found[(Resolve-Path $Target).Path] = 1
} else {
    $places = @("$env:USERPROFILE\Downloads", "$env:USERPROFILE\Desktop", "$env:USERPROFILE\Documents", "$env:USERPROFILE\OneDrive\Desktop", "$env:USERPROFILE\OneDrive\Documents", 'C:\sm2-port', 'C:\HERO-VR', (Split-Path -Parent $here))
    Say 'Looking for your HERO VR mod folders...'
    foreach ($pl in $places) {
        if (-not (Test-Path -LiteralPath $pl)) { continue }
        Get-ChildItem -LiteralPath $pl -Filter 'launch-game-vr.ps1' -File -Recurse -Depth 5 -ErrorAction SilentlyContinue |
            Where-Object { $_.Directory.Name -eq 'tools' -and $_.FullName -notlike "$here*" } |
            ForEach-Object { $found[$_.Directory.Parent.FullName] = 1 }
    }
}
$roots = @($found.Keys | Sort-Object { Played $_ } -Descending)
if (-not $roots.Count) { Fail 'no HERO VR mod folder found. Run it as: INSTALL-TEST-BUILD.bat "C:\path\to\your\mod folder" (the folder with tools\launch-game-vr.ps1 in it).' }
Say ''
Say 'HERO VR mod folders found (newest play first):'
for ($i = 0; $i -lt $roots.Count; $i++) {
    $note = ''
    if ($roots[$i] -match 'backup') { $note += '  [a BACKUP copy]' }
    if (Test-Path (Join-Path $roots[$i] 'HERO-VR-TEST-BUILD.txt')) { $note += '  [' + (Get-Content (Join-Path $roots[$i] 'HERO-VR-TEST-BUILD.txt') -TotalCount 1) + ']' }
    Say ("  [{0}] {1}   (last played {2:yyyy-MM-dd HH:mm}){3}" -f ($i + 1), $roots[$i], (Played $roots[$i]), $note)
}
$root = $roots[0]
if (-not $Yes) {
    Say ''
    Say "The test build goes into [1] - the folder you played from last:" 'Yellow'
    Say "    $root" 'Yellow'
    $a = Read-Host 'Press Enter for [1], type another number, or N to cancel'
    if ($a -match '^[nN]') { Fail 'cancelled, nothing changed.' }
    if ($a -match '^\d+$') { $k = [int]$a; if ($k -lt 1 -or $k -gt $roots.Count) { Fail "there is no [$k]." }; $root = $roots[$k - 1] }
}
if (Get-Process -Name 'Spider-Man2', 'SM2VRLauncher' -ErrorAction SilentlyContinue) {
    Say 'Close the game and the HERO VR launcher first.' 'Yellow'
    if (-not $Yes) { Read-Host 'Then press Enter' | Out-Null }
    if (Get-Process -Name 'Spider-Man2', 'SM2VRLauncher' -ErrorAction SilentlyContinue) { Fail 'the game or the launcher is still open.' }
}

# 3. Copy, keeping what is replaced
$keep = Join-Path $root '_before-0.0.72-test'
$kept = 0
foreach ($e in $entries) {
    $rel = $e[1] -replace '/', '\'
    $to = Join-Path $root $rel
    if ((Test-Path -LiteralPath $to) -and -not (Test-Path -LiteralPath (Join-Path $keep $rel))) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent (Join-Path $keep $rel)) | Out-Null
        Copy-Item -LiteralPath $to -Destination (Join-Path $keep $rel) -Force; $kept++
    }
}
foreach ($e in $entries) {
    $rel = $e[1] -replace '/', '\'
    $to = Join-Path $root $rel
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $to) | Out-Null
    Copy-Item -LiteralPath (Join-Path $payload $rel) -Destination $to -Force
}
$wrong = @()
foreach ($e in $entries) {
    $to = Join-Path $root ($e[1] -replace '/', '\')
    if ((Get-FileHash -LiteralPath $to -Algorithm SHA256).Hash.ToLower() -ne $e[0]) { $wrong += $e[1] }
}
if ($wrong.Count) { Fail "$($wrong.Count) files did not copy (is a file open or read-only?): $($wrong[0])" }
Say ''
Say "INSTALLED into $root" 'Green'
Say "  $($entries.Count) files copied and checked (sha256). $kept replaced files kept in $keep" 'Green'
Say ('  In the console you will see: ' + ($title -replace ' - .*', '')) 'Cyan'
# Test build 11: build 10's report came from a session that still ran build 9 (same DLL hashes), so the
# wrist's number is named here, and every other mod folder is listed as NOT updated.
if ($title -match 'test (\S+) - private test build (\d+)') {
    Say ("  On the wrist (bottom line) you must see: TEST {0} {1}" -f $matches[2], $matches[1]) 'Cyan'
    Say '  If the wrist shows anything else, the game started from another folder.' 'Yellow'
}
foreach ($o in $roots) { if ($o -ne $root) { Say "  NOT updated (do not start HERO VR from here): $o" 'Yellow' } }

# 4. That folder's own launcher
if ($NoLaunch) { exit 0 }
$exe = Join-Path $root 'tools\launcher\SM2VRLauncher.exe'
$cmd = $null
$d = Get-Item -LiteralPath $root
$i = 0
while ($d -and $i -lt 3) {
    foreach ($n in 'SM2VRLauncher.exe', 'HERO VR Launcher.cmd') {
        $c = Join-Path $d.FullName $n
        if (-not $cmd -and (Test-Path -LiteralPath $c)) { $cmd = $c }
    }
    if ($cmd) { break }
    $d = $d.Parent; $i++
}
Say ''
if (Test-Path -LiteralPath $exe) {
    Say "Starting this folder's launcher: $exe"
    Say 'From now on start HERO VR from this folder (or run this installer again: it starts the right launcher).'
    Start-Process -FilePath $exe -WorkingDirectory (Split-Path -Parent $exe)
} elseif ($cmd) {
    Say "Starting: $cmd"
    Say "If the launcher's mod folder is a different one, click Mod folder and pick: $root" 'Yellow'
    Start-Process -FilePath $cmd -WorkingDirectory (Split-Path -Parent $cmd)
} else {
    Say "Start HERO VR as usual; in the launcher click Mod folder and pick: $root" 'Yellow'
}
