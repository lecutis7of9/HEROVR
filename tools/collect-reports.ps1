# Update 28.1: collects what to send after a session into reports\send-me.zip and prints its path:
# the newest game-vr-*.json report, its console and game logs (game-vr-*-console.log, -game.log;
# else the newest game-vr-*.log), and address-finder-latest.json / .txt (Update 32: and
# address-finder-latest-ortho.txt) if ADDRESS FINDER ran.
# Run from anywhere: powershell -ExecutionPolicy Bypass -File C:\sm2-port\tools\collect-reports.ps1
# It only reads the reports folder and writes send-me.zip there (the old send-me.zip is replaced).
# 0.0.74 test 3: and the game's newest crash dump (.mdmp) from Documents\Marvel's Spider-Man 2 (-NoCrashDump:
# without it; the launcher's Privacy > Crash dump in reports passes that when it is off).
# 0.0.75: only a dump written after this session started (the report's and its logs' earliest creation time); an
# older one is left out and said why. The report's logs include its -prompts.log (PROMPTS LOG, the fly notes).
# 0.0.76 SM1: the SM1 add-on's own files from %APPDATA%\HERO VR SM1 (layer.log, addon.log, live.txt, addon.ini) and
# its research scan (scan\sm1scan-*.jsonl/.bin/.txt, the newest 2 sessions); an SM1-only player has no reports
# folder yet, so it is made instead of stopping.
param([switch]$NoCrashDump)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$reports = Join-Path $root 'reports'
if (-not (Test-Path -LiteralPath $reports)) {
    # No SM2 session yet (SM1 only): the folder is made so send-me.zip has a place; nothing else is written.
    New-Item -ItemType Directory -Path $reports -Force | Out-Null
}
$files = @()
$sessionStart = $null
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
    # 0.0.75: when this session started, for the crash dump (the oldest of the report and its logs).
    $sessionStart = (@($report) + @($logs) | Where-Object { $_ } | ForEach-Object { $_.CreationTime } |
        Sort-Object | Select-Object -First 1)
    $prompts = $logs | Where-Object { $_.Name -like '*-prompts.log' } | Select-Object -First 1
    if ($prompts) { Write-Host ('Prompts log: ' + $prompts.Name) }
} else {
    Write-Host 'No game-vr report yet (game-vr-*.json); collecting what there is.'
}
$files += Get-ChildItem -LiteralPath $reports -Filter 'address-finder-latest*' -File | ForEach-Object { $_.FullName }
# Update 37: the newest WORLD LOG journal (NPC talk, spawns, missions; read-only notes).
$files += Get-ChildItem -LiteralPath $reports -Filter 'world-trace-*.jsonl' -File |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1 | ForEach-Object { $_.FullName }
# 0.0.74 test 10: the WINDOW PROBE logs of the last 3 sessions (read-only; tools/window_probe.py).
$files += Get-ChildItem -LiteralPath $reports -Filter 'window-probe-*.jsonl' -File |
    Sort-Object LastWriteTime -Descending | Select-Object -First 3 | ForEach-Object { $_.FullName }
# 0.0.76 SM1: the add-on's logs and settings, and its research scan's newest 2 sessions (read only).
try {
    if ($env:APPDATA) {
        $sm1 = Join-Path $env:APPDATA 'HERO VR SM1'
        if (Test-Path -LiteralPath $sm1) {
            $files += Get-ChildItem -LiteralPath $sm1 -File |
                Where-Object { @('layer.log', 'addon.log', 'live.txt', 'addon.ini') -contains $_.Name } |
                ForEach-Object { $_.FullName }
            $sm1Scan = Join-Path $sm1 'scan'
            if (Test-Path -LiteralPath $sm1Scan) {
                $scanSessions = @(Get-ChildItem -LiteralPath $sm1Scan -Filter 'sm1scan-*.jsonl' -File |
                    Sort-Object LastWriteTime -Descending | Select-Object -First 2)
                foreach ($s in $scanSessions) {
                    $files += Get-ChildItem -LiteralPath $sm1Scan -Filter ($s.BaseName + '.*') -File | ForEach-Object { $_.FullName }
                }
                if ($scanSessions) { Write-Host ('SM1 scan: ' + (($scanSessions | ForEach-Object { $_.BaseName }) -join ', ')) }
            }
        }
    }
} catch { Write-Host ('SM1 add-on files not added: ' + $_.Exception.Message) }
# 0.0.74 test 3: the game's newest crash dump (the game writes one when it crashes; it says where in its log).
if (-not $NoCrashDump) {
    try {
        $gameDocs = Join-Path ([Environment]::GetFolderPath('MyDocuments')) "Marvel's Spider-Man 2"
        if (Test-Path -LiteralPath $gameDocs) {
            $dump = Get-ChildItem -LiteralPath $gameDocs -Filter '*.mdmp' -File -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTime -Descending | Select-Object -First 1
            if ($dump -and -not $sessionStart) {
                Write-Host ('Crash dump not added: no session report to date it against (newest ' + $dump.Name + ', ' +
                    $dump.LastWriteTime.ToString('yyyy-MM-dd HH:mm') + ').')
            } elseif ($dump -and $dump.LastWriteTime -le $sessionStart) {
                Write-Host ('Crash dump not added: none newer than this session (started ' +
                    $sessionStart.ToString('yyyy-MM-dd HH:mm') + '; newest ' + $dump.Name + ', ' +
                    $dump.LastWriteTime.ToString('yyyy-MM-dd HH:mm') + ').')
            } elseif ($dump -and $dump.Length -le 256MB) {
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
