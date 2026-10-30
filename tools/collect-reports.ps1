# Update 28.1: collects what to send after a session into reports\send-me.zip and prints its path:
# the newest game-vr-*.json report, its console and game logs (game-vr-*-console.log, -game.log;
# else the newest game-vr-*.log), and address-finder-latest.json / .txt (Update 32: and
# address-finder-latest-ortho.txt) if ADDRESS FINDER ran.
# Run from anywhere: powershell -ExecutionPolicy Bypass -File C:\sm2-port\tools\collect-reports.ps1
# It only reads the reports folder and writes send-me.zip there (the old send-me.zip is replaced).
# 0.0.74 test 3: and the game's newest crash dump (.mdmp) from Documents\Marvel's Spider-Man 2 (-NoCrashDump:
# without it; the launcher's Privacy > Crash dump in reports passes that when it is off).
param([switch]$NoCrashDump)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$reports = Join-Path $root 'reports'
if (-not (Test-Path -LiteralPath $reports)) { throw "No reports folder at $reports - play a session first." }
$files = @()
# Update 38.5: the version these reports came from, first.
$versionFile = Join-Path $root 'VERSION'
if (Test-Path -LiteralPath $versionFile) { Write-Host ('SM2 VR Version ' + (Get-Content -LiteralPath $versionFile -TotalCount 1).Trim() + ' - Made by Agent Mulder') }
$report = Get-ChildItem -LiteralPath $reports -Filter 'game-vr-*.json' -File |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($report) {
    $files += $report.FullName
    $logs = @(Get-ChildItem -LiteralPath $reports -Filter ($report.BaseName + '*.log') -File)
    if (-not $logs) {
        $logs = @(Get-ChildItem -LiteralPath $reports -Filter 'game-vr-*.log' -File |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1)
    }
    $files += $logs | ForEach-Object { $_.FullName }
} else {
    Write-Host 'No game-vr report yet (game-vr-*.json); collecting what there is.'
}
$files += Get-ChildItem -LiteralPath $reports -Filter 'address-finder-latest*' -File | ForEach-Object { $_.FullName }
# Update 37: the newest WORLD LOG journal (NPC talk, spawns, missions; read-only notes).
$files += Get-ChildItem -LiteralPath $reports -Filter 'world-trace-*.jsonl' -File |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1 | ForEach-Object { $_.FullName }
# 0.0.74 test 3: the game's newest crash dump (the game writes one when it crashes; it says where in its log).
if (-not $NoCrashDump) {
    try {
        $gameDocs = Join-Path ([Environment]::GetFolderPath('MyDocuments')) "Marvel's Spider-Man 2"
        if (Test-Path -LiteralPath $gameDocs) {
            $dump = Get-ChildItem -LiteralPath $gameDocs -Filter '*.mdmp' -File -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if ($dump -and $dump.Length -le 256MB) {
                $files += $dump.FullName
                Write-Host ('Crash dump: ' + $dump.Name + ' (' + $dump.LastWriteTime.ToString('yyyy-MM-dd HH:mm') + ')')
            } elseif ($dump) {
                Write-Host ('Crash dump not added: ' + $dump.FullName + ' is over 256 MB; send it separately if asked.')
            }
        }
    } catch { Write-Host ('Crash dump not added: ' + $_.Exception.Message) }
}
if (-not $files) { throw "Nothing to collect in $reports." }
$zip = Join-Path $reports 'send-me.zip'
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -LiteralPath $files -DestinationPath $zip -CompressionLevel Optimal
foreach ($f in $files) { Write-Host ('  ' + (Split-Path -Leaf $f)) }
Write-Host $zip
